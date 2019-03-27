/**
* Tencent is pleased to support the open source community by making DCache available.
* Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
* Licensed under the BSD 3-Clause License (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSD-3-Clause
*
* Unless required by applicable law or agreed to in writing, software distributed under
* the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
* either express or implied. See the License for the specific language governing permissions
* and limitations under the License.
*/
#include "PropertyReapThread.h"
#include "PropertyServer.h"
#include "PropertyDbManager.h"

///////////////////////////////////////////////////////////
//

PropertyReapThread::~PropertyReapThread()
{
    if (isAlive())
    {
        terminate();

        getThreadControl().join();
    }
}

void PropertyReapThread::terminate()
{
    TLOGDEBUG("PropertyReapThread::terminate");

    _terminate = true;

    PropertyDbManager::getInstance()->setTerminateFlag(true);

    TC_ThreadLock::Lock lock(*this);

    notifyAll();
}

void PropertyReapThread::run()
{
    TLOGDEBUG("PropertyReapThread::run REAP_INTERVAL:" << REAP_INTERVAL << "|begin ...");

    const string &sClonePath = g_app.getClonePath();
    
    while (!_terminate)
    {
        try
        {
            vector<string> vCloneFiles;
            TC_File::listDirectory(sClonePath, vCloneFiles, false);
            
            sort(vCloneFiles.begin(), vCloneFiles.end());
            
            for (size_t i = 0; (i < vCloneFiles.size()) && !_terminate; ++i)
            {
                string sFileName = TC_File::extractFileName(TC_File::excludeFileExt(vCloneFiles[i]));

                if(sFileName.length() != 12 || TC_Common::isdigit(sFileName) == false)
                {
                    TLOGERROR("PropertyReapThread::run invalid clone file:" << vCloneFiles[i] << endl);
                    continue;
                }

                string sDate = sFileName.substr(0, 8);
                string sFlag = sFileName.substr(8, 4);

                PropertyMsg mPropMsg;
                getPropertyMsg(vCloneFiles[i], mPropMsg);
                if (!mPropMsg.empty())
                {
                    PropertyDbManager::getInstance()->insert2Db(mPropMsg, sDate, sFlag);
                }

                if (_terminate)
                {
                    break;
                }

                TC_File::removeFile(vCloneFiles[i], false);
            }
        }
        catch (exception &ex)
        {
            TLOGERROR("PropertyReapThread::run exception:" << ex.what() << endl);
        }
        catch (...)
        {
            TLOGERROR("PropertyReapThread::run unkown exception" << endl);
        }

        TC_ThreadLock::Lock lock(*this);
        timedWait(REAP_INTERVAL);
    }
}

void PropertyReapThread::getPropertyMsg(const string &sCloneFile, PropertyMsg &mPropMsg)
{
    TLOGDEBUG("PropertyReapThread::getPropertyMsg sCloneFile:" << sCloneFile << "|begin ..." << endl);

    try
    {
        PropertyHashMap tHashMap;
        size_t iSize = TC_File::getFileSize(sCloneFile);
        tHashMap.initStore(sCloneFile.c_str(), iSize);
        
        TLOGDEBUG("PropertyReapThread::getPropertyMsg tHashMap:" << tHashMap.desc() << endl);
        
        size_t iCount = 0;
        
        PropertyHashMap::lock_iterator it = tHashMap.beginSetTime();
        while (it != tHashMap.end())
        {
            PropKey key;
            PropValue value;
            
            int ret = it->get(key, value);
            
            if (ret == TC_HashMap::RT_OK)
            {
                ++iCount;
                mPropMsg[key] = value;
            }

            ++it;
        }

        TLOGDEBUG("PropertyReapThread::getPropertyMsg get total size:" << iCount << endl);
    }
    catch (exception &ex)
    {
        TLOGERROR("PropertyReapThread::getPropertyMsg exception:" << ex.what() << endl);

        string sMsg("PropertyReapThread::getPropertyMsg Clone File:");
        sMsg += sCloneFile;
        sMsg += "|exception:";
        sMsg += ex.what();
        sendAlarmSMS(sMsg);
    }
}

void PropertyReapThread::sendAlarmSMS(const string &sMsg)
{
    string sInfo = "ERROR:" + ServerConfig::LocalIp + "_" + sMsg;
    
    TARS_NOTIFY_ERROR(sInfo);
}

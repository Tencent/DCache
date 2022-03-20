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
#include "UnpackTable.h"

int g_iSlaveFlag = 0;//备机可读的轮询标志

namespace DCache
{

    ////////////////////////////////////////
    UnpackTable::Entry::Entry()
        : value(0)
    {
    }


    //////////////////////////////////////////////////////
    UnpackTable::UnpackTable() : _lrInfo(NULL), _lrInfo1(NULL), _init(false), _iLastLoadTime(0),
        _pageSize(__pageSize), _allPageCount(__allPageCount), _reloadTime(__reloadTime)
    {
        _hash = new NormalHash();
    }

    UnpackTable::UnpackTable(const UnpackTable& r) : _lrInfo(NULL), _lrInfo1(NULL), _init(false),
        _iLastLoadTime(0), _pageSize(__pageSize), _allPageCount(__allPageCount),
        _reloadTime(__reloadTime)
    {
        _hash = new NormalHash();
        string tmpServer = r.getSelfServer();
        _pageSize = getPageSize();
        _reloadTime = getReloadTime();
        init(r.getPackTable(), tmpServer, _pageSize, _reloadTime);
    }

    UnpackTable::~UnpackTable()
    {
        delete _lrInfo;
        _lrInfo = NULL;

        delete _lrInfo1;
        _lrInfo1 = NULL;

        delete _hash;
        _hash = NULL;
    }

    //////////////////////////////////////////////////////
    int UnpackTable::init(const PackTable& packTable, const string& sServerName,
        uint32_t pageSize, int64_t reloadTime, const vector<TransferInfo>& transferingInfoList)
    {
        LocalRouterInfo* tmpLRInfo = NULL;
        __UNPACK_TRY__

        uint32_t uPageSize = pageSize > 0 ? pageSize : _pageSize;
        uint32_t uAllPageCount = __max / uPageSize + (__max % uPageSize > 0 ? 1 : 0);
        int64_t i64ReloadTime = reloadTime > 0 ? reloadTime : _reloadTime;

        tmpLRInfo = new LocalRouterInfo(uAllPageCount);

        // 加载服务器组及服务器信息
        map<string, GroupInfo>::const_iterator it;
        for (it = packTable.groupList.begin(); it != packTable.groupList.end(); ++it)
        {
            tmpLRInfo->groupMap[it->second.id] = it->second;
            for (map<string, vector<string> >::const_iterator it2 = it->second.idcList.begin(); it2 != it->second.idcList.end(); ++it2)
            {
                for (size_t i = 0; i < it2->second.size(); i++)
                {
                    tmpLRInfo->serverNameMap[it2->second[i]] = it->second.id;
                }
            }

        }

        string selfGroupName;	// 找出sServerName所在服务器组名
        int iRet = 0;
        if (!sServerName.empty())
        {
            iRet = RET_NOT_FOUND_SERVER;
            map<string, int>::const_iterator it_s;
            if ((it_s = tmpLRInfo->serverNameMap.find(sServerName)) != tmpLRInfo->serverNameMap.end())
            {
                iRet = RET_NOT_FOUND_GROUP;
                map<int, GroupInfo>::const_iterator it_g;
                if ((it_g = tmpLRInfo->groupMap.find(it_s->second)) != tmpLRInfo->groupMap.end())
                {
                    selfGroupName = it_g->second.groupName;
                    iRet = RET_SUCC;
                }
            }
            if (iRet != RET_SUCC)
            {
                delete tmpLRInfo;
                return iRet;
            }
        }

        // 加载路由信息
        for (size_t i = 0; i < packTable.recordList.size(); i++)
        {
            Entry entry;
            entry.value = 0;
            map<string, GroupInfo>::const_iterator it;
            if ((it = packTable.groupList.find(packTable.recordList[i].groupName))
                != packTable.groupList.end())	// 路由记录对应的服务器组存在
            {
                entry.setIndex(it->second.id);

                tmpLRInfo->routerGroupMap[it->second.id] = 0;
            }
            else
            {
                delete tmpLRInfo;
                return RET_NOT_FOUND_GROUP;
            }

            // 路由记录对应的服务器组名与本机所在组名相同，则该页为本机服务范围
            if (packTable.recordList[i].groupName == selfGroupName)
            {
                entry.setSelf(true);
            }

            int k = packTable.recordList[i].toPageNo;
            for (int j = packTable.recordList[i].fromPageNo; j <= k; ++j)
            {
                memcpy(&tmpLRInfo->entry[j], &entry, sizeof(entry));
            }
        }

        if (!transferingInfoList.empty())
        {
            vector<TransferInfo>::const_iterator it = transferingInfoList.begin();
            for (; it != transferingInfoList.end(); ++it)
            {
                for (int i = it->fromPageNo; i <= it->toPageNo; ++i)
                {
                    tmpLRInfo->entry[i].setTransfering(true);
                    map<string, GroupInfo>::const_iterator it1;
                    if ((it1 = packTable.groupList.find(it->transGroupName))
                        != packTable.groupList.end())	// 路由记录对应的迁移目的服务器组存在
                    {
                        tmpLRInfo->entry[i].setTransIndex(it1->second.id);
                    }
                    else
                    {
                        delete tmpLRInfo;
                        return RET_NOT_FOUND_GROUP;
                    }

                    // 如果本机就是迁移的目的服务器，则也将该页设置为自己的服务范围
                    if (it->transGroupName == selfGroupName)
                    {
                        tmpLRInfo->entry[i].setSelf(true);
                    }
                }
            }
        }

        tmpLRInfo->packTable = packTable;

        // 切换指针
        TC_ThreadLock::Lock lock(_lock);
        int64_t nowus = TC_Common::now2us();
        if (nowus - _iLastLoadTime < i64ReloadTime)
        {
            usleep(i64ReloadTime - nowus + _iLastLoadTime);
        }

        LocalRouterInfo* oldLRInfo = _lrInfo1, *newLRInfo = tmpLRInfo;
        tmpLRInfo = NULL;
        _lrInfo1 = _lrInfo;
        _lrInfo = newLRInfo;
        delete oldLRInfo;

        // 记录初始化参数
        if (!_init)
        {
            _selfServer = sServerName;
            _init = true;
            _pageSize = uPageSize;
            _allPageCount = uAllPageCount;
            _reloadTime = i64ReloadTime;
        }

        // 记录最后一次加载时间
        _iLastLoadTime = TC_Common::now2us();

        return RET_SUCC;

        __UNPACK_CATCH__

            if (tmpLRInfo != NULL)
            {
                delete tmpLRInfo;
            }

        return RET_EXCEPTION;
    }

    int UnpackTable::reload(const PackTable& packTable, const vector<TransferInfo>& transferingInfoList)
    {
        return init(packTable, _selfServer, _pageSize, _reloadTime, transferingInfoList);
    }

    int UnpackTable::getMasterByHash(uint32_t hash, ServerInfo& serverInfo) const
    {
        __UNPACK_TRY__

            int iRet = RET_SUCC;
        // 取得页号
        uint32_t pageNo = hash / _pageSize;
        assert(pageNo <= _allPageCount);
        LocalRouterInfo * lrInfTmp = _lrInfo;
        // 根据页号获取所属服务器组编号
        int groupId = lrInfTmp->entry[pageNo].getIndex();
        map<int, GroupInfo>::const_iterator it = lrInfTmp->groupMap.find(groupId);
        if (it != lrInfTmp->groupMap.end())
        {
            if (it->second.accessStatus == 1)
                return RET_GROUP_READONLY;
            serverInfo = lrInfTmp->packTable.serverList[it->second.masterServer];
            if (serverInfo.serverName.empty())	// 服务器名为空则说明服务器不存在
                return RET_NOT_FOUND_SERVER;
        }
        else
        {
            iRet = RET_NOT_FOUND_GROUP;
        }

        return iRet;

        __UNPACK_CATCH__

            return RET_EXCEPTION;
    }

    int UnpackTable::getAllMasters(vector<ServerInfo>& masterServers) const
    {
        __UNPACK_TRY__

        int iRet = RET_SUCC;
        masterServers.clear();
        LocalRouterInfo * lrInfTmp = _lrInfo;
        // 遍历groupMap，获取所有masterServer
        map<int, GroupInfo>::const_iterator it;
        for (it = lrInfTmp->groupMap.begin(); it != lrInfTmp->groupMap.end(); it++)
        {
            if (!it->second.masterServer.empty())
            {
                masterServers.push_back(lrInfTmp->packTable.serverList[it->second.masterServer]);
            }
            else
            {   // 程序如果运行到此else分支，表示该组没有主服务器，这是个严重的错误！
                iRet = RET_NOT_FOUND_SERVER;	// 找不到仍然继续
            }
        }

        return iRet;

        __UNPACK_CATCH__

        return RET_EXCEPTION;
    }

    int UnpackTable::getAllIdcServer(const string &sIdc, vector<ServerInfo> &servers) const
    {
        __UNPACK_TRY__

        int iRet = RET_SUCC;
        servers.clear();
        LocalRouterInfo * lrInfTmp = _lrInfo;
        // 遍历所有有路由的组
        map<int, int>::const_iterator itRouter;
        for (itRouter = lrInfTmp->routerGroupMap.begin(); itRouter != lrInfTmp->routerGroupMap.end(); itRouter++)
        {
            map<int, GroupInfo>::const_iterator it = lrInfTmp->groupMap.find(itRouter->first);
            if (it == lrInfTmp->groupMap.end())
            {
                TLOG_ERROR("[UnpackTable::getAllIdcServer] can not find group id:" << itRouter->first << endl);
                return -1;
            }
            map<string, vector<string> >::const_iterator it_idc = it->second.idcList.find(sIdc);
            if (it_idc != it->second.idcList.end())
            {
                //只读状态，把对主机的读都切到备机
                if (it->second.accessStatus == 1)
                {
                    if (it_idc->second.size() > 1)
                    {
                        servers.push_back(lrInfTmp->packTable.serverList[it_idc->second[1]]);
                    }
                    else
                        servers.push_back(lrInfTmp->packTable.serverList[it_idc->second[0]]);
                }
                //镜像不可用状态，把对镜像的读切换到备机
                else if (it->second.accessStatus == 2)
                {
                    ServerInfo serverInfo = lrInfTmp->packTable.serverList[it->second.masterServer];
                    if (sIdc != serverInfo.idc)
                    {
                        //查询镜像，先要检查镜像服务状态。如果没有镜像备机并且状态不为0，则切到备机
                        ServerInfo mirrServerInfo = _lrInfo->packTable.serverList[it_idc->second[0]];
                        if ((it_idc->second.size() == 1) && (mirrServerInfo.status != 0))
                        {
                            map<string, vector<string> >::const_iterator it_idc_master = it->second.idcList.find(serverInfo.idc);
                            if (it_idc_master->second.size() > 1)
                                servers.push_back(lrInfTmp->packTable.serverList[it_idc_master->second[1]]);
                            else
                                servers.push_back(lrInfTmp->packTable.serverList[it_idc_master->second[0]]);
                        }
                        else
                            servers.push_back(mirrServerInfo);
                    }
                    else
                        servers.push_back(lrInfTmp->packTable.serverList[it_idc->second[0]]);
                }
                else
                {
                    servers.push_back(lrInfTmp->packTable.serverList[it_idc->second[0]]); //?
                }
            }
            else
            {
                servers.push_back(lrInfTmp->packTable.serverList[it->second.masterServer]);
            }
        }

        if (servers.size() == 0)
        {
            TLOG_ERROR("[UnpackTable::getAllIdcServer] can not find groups!" << endl);
            return -1;
        }

        return iRet;

        __UNPACK_CATCH__

            return RET_EXCEPTION;
    }

    int UnpackTable::getGroup(const string& sServerName, GroupInfo& group) const
    {
        __UNPACK_TRY__

        int iRet = RET_NOT_FOUND_SERVER;

        LocalRouterInfo * lrInfTmp = _lrInfo;

        // 根据服务器名找出所在服务器组
        map<string, int>::const_iterator it_s;
        if ((it_s = lrInfTmp->serverNameMap.find(sServerName)) != lrInfTmp->serverNameMap.end())
        {
            iRet = RET_NOT_FOUND_GROUP;
            map<int, GroupInfo>::const_iterator it_g;
            if ((it_g = lrInfTmp->groupMap.find(it_s->second)) != lrInfTmp->groupMap.end())
            {
                group = it_g->second;
                iRet = RET_SUCC;
            }
        }

        return iRet;

        __UNPACK_CATCH__

        return RET_EXCEPTION;
    }


    int UnpackTable::getTrans(int PageNo, ServerInfo& srcServer,
        ServerInfo& destServer)
    {
        __UNPACK_TRY__

        int iRet = RET_SUCC;
        LocalRouterInfo * lrInfTmp = _lrInfo;
        if ((uint32_t)PageNo <= _allPageCount)
        {
            // 获取当前所属服务器信息
            uint32_t id = lrInfTmp->entry[PageNo].getIndex();
            map<int, GroupInfo>::const_iterator it_src_group = lrInfTmp->groupMap.find(id);
            if (it_src_group != lrInfTmp->groupMap.end())
            {
                if (it_src_group->second.masterServer.empty())
                    iRet = RET_NOT_FOUND_SRC_MASTER;

                srcServer = lrInfTmp->packTable.serverList[it_src_group->second.masterServer];
            }
            else
            {
                iRet = RET_NOT_FOUND_SRC_GROUP;
            }

            //  如果正在迁移，则获取迁移目的服务器信息
            if (lrInfTmp->entry[PageNo].getTransfering())
            {
                id = lrInfTmp->entry[PageNo].getTransIndex();
                map<int, GroupInfo>::const_iterator it_dest_group = lrInfTmp->groupMap.find(id);
                if (it_dest_group != lrInfTmp->groupMap.end())
                {
                    if (it_dest_group->second.masterServer.empty())
                        iRet = RET_NOT_FOUND_DEST_MASTER;

                    destServer = lrInfTmp->packTable.serverList[it_dest_group->second.masterServer];
                }
                else
                {
                    iRet = RET_NOT_FOUND_DEST_GROUP;
                }
            }
            else
            {
                iRet = RET_NOT_TRANSFERING;
            }
        }
        else
        {
            iRet = RET_NOT_FOUND_TRANS;
        }

        return iRet;

        __UNPACK_CATCH__

        return RET_EXCEPTION;
    }

    bool UnpackTable::isTransSrc(int PageNo, const string& sServerName)
    {
        ServerInfo srcServer;
        ServerInfo destServer;

        getTrans(PageNo, srcServer, destServer);

        if (srcServer.serverName == sServerName)
            return true;

        return false;
    }

    int UnpackTable::getBakSource(const string& sServerName, ServerInfo &bakSourceServer)
    {
        __UNPACK_TRY__

        LocalRouterInfo * lrInfTmp = _lrInfo;
        int iRet = RET_NOT_FOUND_SERVER;

        map<string, int>::const_iterator it_s;
        if ((it_s = lrInfTmp->serverNameMap.find(sServerName)) != lrInfTmp->serverNameMap.end())
        {
            iRet = RET_NOT_FOUND_GROUP;
            map<int, GroupInfo>::const_iterator it_g;
            if ((it_g = lrInfTmp->groupMap.find(it_s->second)) != lrInfTmp->groupMap.end())
            {
                map<string, string>::const_iterator it_bs;
                if ((it_bs = it_g->second.bakList.find(sServerName)) != it_g->second.bakList.end())
                {
                    if (it_bs->second != "")
                    {
                        iRet = RET_SUCC;
                        bakSourceServer = lrInfTmp->packTable.serverList[it_bs->second];
                    }
                    else
                    {
                        iRet = RET_NOT_FOUND_SERVER;
                    }
                }
                else
                {
                    iRet = RET_NOT_FOUND_SERVER;
                }
            }
        }

        return iRet;

        __UNPACK_CATCH__

            return RET_EXCEPTION;
    }

    int UnpackTable::getVersion()
    {
        return _lrInfo->packTable.info.version;	// 返回版本号
    }

    bool UnpackTable::isMySelfByHash(uint32_t hash) const
    {
        uint32_t pageNo = hash / _pageSize;
        assert(pageNo <= _allPageCount);

        return _lrInfo->entry[pageNo].getSelf();
    }

    bool UnpackTable::isTransferingByHash(uint32_t hash) const
    {
        uint32_t pageNo = hash / _pageSize;
        assert(pageNo <= _allPageCount);

        return _lrInfo->entry[pageNo].getTransfering();
    }


    void UnpackTable::print(ostream& os) const
    {
        __UNPACK_TRY__

        _lrInfo->packTable.display(os);

        __UNPACK_CATCH__

        return;
    }

    void UnpackTable::clear()
    {
        __UNPACK_TRY__

        LocalRouterInfo* oldLRInfo = _lrInfo;
        _lrInfo = new LocalRouterInfo(_allPageCount);
        delete oldLRInfo;

        oldLRInfo = _lrInfo1;
        _lrInfo1 = new LocalRouterInfo(_allPageCount);
        delete oldLRInfo;

        _selfServer.clear();

        __UNPACK_CATCH__

        return;
    }

    int UnpackTable::fromFile(const string& fileName, const string& sServerName)
    {
        __UNPACK_TRY__

        string sContent = TC_File::load2str(fileName);
        tars::TarsInputStream<tars::BufferReader> _is;
        _is.setBuffer(sContent.c_str(), sContent.length());
        PackTable tmpPackTable;
        _is.read(tmpPackTable, 0, true);
        int iRet = init(tmpPackTable, sServerName, _pageSize, _reloadTime);
        return iRet;

        __UNPACK_CATCH__

        return RET_EXCEPTION;
    }

    int UnpackTable::toFile(const string& fileName) const
    {
        __UNPACK_TRY__

            ofstream outputStream(fileName.c_str());
        tars::TarsOutputStream<tars::BufferWriter> _os;
        _os.write(_lrInfo->packTable, 0);
        outputStream.write(_os.getBuffer(), _os.getLength());
        outputStream.close();
        return 0;

        __UNPACK_CATCH__

            return RET_EXCEPTION;
    }

    string UnpackTable::toString() const
    {
        __UNPACK_TRY__

        ostringstream os;
        _lrInfo->packTable.display(os);
        return os.str();

        __UNPACK_CATCH__

        return "";
    }

    size_t UnpackTable::hashKey(uint32_t key) const
    {
        return _hash->HashRawInt(key);
    }

    size_t UnpackTable::hashKey(uint64_t key) const
    {
        return _hash->HashRawLong(key);
    }

    size_t UnpackTable::hashKey(string key) const
    {
        return _hash->HashRawString(key);
    }
}


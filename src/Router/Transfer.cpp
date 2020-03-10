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
#include "Transfer.h"
#include "DbHandle.h"
#include "OuterProxyFactory.h"
#include "RouterServer.h"

extern RouterServer g_app;

namespace DCache
{
Transfer::Transfer(std::shared_ptr<OuterProxyFactory> outerProxy) : _outerProxy(outerProxy) {}

void Transfer::init(std::shared_ptr<DbHandle> dbHandle)
{
    _transTo = g_app.getGlobalConfig().getTransferTimeout(3000);
    _defragDbInterval = g_app.getGlobalConfig().getTransferDefragDbInterval(50);
    _retryTranInterval = g_app.getGlobalConfig().getRetryTransferInterval(1);
    _retryTranMaxTimes = g_app.getGlobalConfig().getRetryTransferMaxTimes(3);
    _transferPagesOnceMax = g_app.getGlobalConfig().getTransferPagesOnce(5);
    if (_transferPagesOnceMax > 10) _transferPagesOnceMax = 10;
    _transferPagesOnce = _transferPagesOnceMax;
    _dbHandle = dbHandle;
}

void Transfer::reloadConf()
{
    TLOGDEBUG("Transfer reload config ..." << endl);
    init(_dbHandle);
}

int Transfer::updateTransferingRecord(const TransferInfo *transInfoNew,
                                      const TransferInfo *transInfoComplete)
{
    ostringstream os;
    if (transInfoComplete)
    {
        os << "complete ";
        transInfoComplete->displaySimple(os);
        os << " ,";
    }
    if (transInfoNew)
    {
        os << "begin ";
        transInfoNew->displaySimple(os);
    }

    int ret = eSucc;
    bool cleanSrc = false, cleanDest = false, cleanSrcHasInc = false, cleanDestHasInc = false;
    do
    {
        int rc = _dbHandle->updateTransferingRecord(transInfoNew,
                                                    transInfoComplete,
                                                    cleanSrc,
                                                    cleanDest,
                                                    cleanSrcHasInc,
                                                    cleanDestHasInc,
                                                    0);
        if (rc == 0)
        {
            ret = e_Succ;
            break;
        }
        else if (rc == 1)
        {
            sleep(1);
            rc = notifyTransServerClean(transInfoNew->moduleName, transInfoNew->groupName);
            if (rc == e_Succ)
            {
                cleanSrc = true;
                DAY_ERROR << "updateTransferingRecord while rc == 1" << endl;
                continue;
            }
            else
            {
                rc = _dbHandle->updateTransferingRecord(transInfoNew->moduleName,
                                                        transInfoNew->groupName);
                if (rc != 0)
                {
                    DAY_ERROR << "Transfer::updateTransferingRecord update record incorrect: "
                              << transInfoNew->moduleName << endl;
                    TARS_NOTIFY_WARN(
                        string("Transfer::updateTransferingRecord|update record incorrect: ") +
                        transInfoNew->moduleName);
                    _outerProxy->reportException("TransError");
                }
                ret = e_NotifySrcServer_Clean_Fail;
                break;
            }
        }
        else if (rc == 2)
        {
            sleep(1);
            rc = notifyTransServerClean(transInfoNew->moduleName, transInfoNew->groupName);
            if (rc == e_Succ)
            {
                cleanSrc = true;
            }
            else
            {
                rc = _dbHandle->updateTransferingRecord(transInfoNew->moduleName,
                                                        transInfoNew->groupName,
                                                        transInfoNew->transGroupName);
                if (rc != 0)
                {
                    DAY_ERROR << "Transfer::updateTransferingRecord update record incorrect: "
                              << transInfoNew->moduleName << endl;
                    TARS_NOTIFY_WARN(
                        string("Transfer::updateTransferingRecord|update record incorrect: ") +
                        transInfoNew->moduleName);
                    _outerProxy->reportException("TransError");
                }
                ret = e_NotifySrcServer_Clean_Fail;
                break;
            }

            rc = notifyTransServerClean(transInfoNew->moduleName, transInfoNew->transGroupName);
            if (rc == e_Succ)
            {
                cleanDest = true;
                continue;
            }
            else
            {
                rc = _dbHandle->updateTransferingRecord(transInfoNew->moduleName,
                                                        transInfoNew->groupName,
                                                        transInfoNew->transGroupName);
                if (rc != 0)
                {
                    DAY_ERROR << "Transfer::updateTransferingRecord update record incorrect: "
                              << transInfoNew->moduleName << endl;
                    TARS_NOTIFY_WARN(
                        string("Transfer::updateTransferingRecord|update record incorrect: ") +
                        transInfoNew->moduleName);
                    _outerProxy->reportException("TransError");
                }
                ret = e_NotifyDestSever_Clean_Fail;
                break;
            }
        }
        else if (rc == 3)
        {
            sleep(1);
            rc = notifyTransServerClean(transInfoNew->moduleName, transInfoNew->transGroupName);
            if (rc == e_Succ)
            {
                cleanDest = true;
                continue;
            }
            else
            {
                rc = _dbHandle->updateTransferingRecord(transInfoNew->moduleName,
                                                        transInfoNew->transGroupName);
                if (rc != 0)
                {
                    DAY_ERROR << "Transfer::updateTransferingRecord update record incorrect: "
                              << transInfoNew->moduleName << endl;
                    TARS_NOTIFY_WARN(
                        string("Transfer::updateTransferingRecord|update record incorrect: ") +
                        transInfoNew->moduleName);
                    _outerProxy->reportException("TransError");
                }
                ret = e_NotifyDestSever_Clean_Fail;
                break;
            }
        }
        else if (rc == 4)
        {
            sleep(1);
            DAY_ERROR << "updateTransferingRecord while rc == 4" << endl;
            continue;
        }
        else
        {
            ret = e_Correspond_Record_Not_Found;
            break;
        }
    } while (1);

    return ret;
}

int Transfer::notifyTransDestServer(const TransferInfo &transferInfo)
{
    int ret = eSucc;
    PROC_BEGIN

    PackTable packTable;
    vector<TransferInfo> transferingInfoList;
    int version;

    int ret =
        _dbHandle->getRouterInfo(transferInfo.moduleName, version, transferingInfoList, packTable);
    if (ret != 0)
    {
        DAY_ERROR << "Transfer::notifyTransDestServer cant find moduleName: "
                  << transferInfo.moduleName << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyTransDestServer|cant find moduleName: ") +
                         transferInfo.moduleName);
        _outerProxy->reportException("TransError");
        ret = e_ModuleName_Not_Found;
        break;
    }
    TLOGDEBUG("version: " << version << ", size: " << transferingInfoList.size() << endl);

    string addr;
    // 获取迁移目的服务器的RouterClent对象名
    ret = getRouterClientJceAddr(packTable, transferInfo.transGroupName, addr);
    if (ret != e_Succ)
    {
//        ret = ret;
        break;
    }

    try
    {
        RouterClientPrx routerClientPrx;
        if (_outerProxy->getProxy(addr, routerClientPrx) != 0)
        {
            ret = e_getProxy_Fail;
            break;
        }

        //异步方式调用
        pthread_mutex_t *mutex = NULL;
        pthread_cond_t *cond = NULL;
        if (_dbHandle->getTransferMutexCond(transferInfo.moduleName,
                                            TC_Common::tostr<pthread_t>(pthread_self()),
                                            &mutex,
                                            &cond) != 0)
        {
            ret = e_toTransferStart_Fail;
            break;
        }
        routerClientPrx->tars_async_timeout(_transTo);
        TransferParamPtr param = new TransferParam(mutex, cond, e_toTransfer_RestartSwitch_Fail);
        RouterClientPrxCallbackPtr cb = new RouterClientCallback(param);
        routerClientPrx->async_toTransferStart(
            cb, transferInfo.moduleName, version, transferingInfoList, transferInfo, packTable);

        pthread_mutex_lock(mutex);
        pthread_cond_wait(cond, mutex);
        pthread_mutex_unlock(mutex);
        ret = param->_return;
    }
    catch (const TarsException &ex)
    {
        DAY_ERROR << "Transfer::notifyTransDestServer catch exception: " << ex.what()
                  << " at: " << addr << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyTransDestServer|") + ex.what() + " at: " + addr);
        _outerProxy->reportException("TransError");
        ret = e_toTransferStart_Get_TarsException;
        _outerProxy->deleteProxy(addr);  // 清除代理缓存
    }

    PROC_END

    return ret;
}

int Transfer::notifyTransSrcServer(const TransferInfo &transferInfo)
{
    int ret = eSucc;
    PROC_BEGIN

    PackTable packTable;
    vector<TransferInfo> transferingInfoList;
    int version;

    int rc =
        _dbHandle->getRouterInfo(transferInfo.moduleName, version, transferingInfoList, packTable);
    if (rc != 0)
    {
        DAY_ERROR << "Transfer::notifyTransSrcServer cant find moduleName: "
                  << transferInfo.moduleName << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyTransSrcServer|cant find moduleName: ") +
                         transferInfo.moduleName);
        _outerProxy->reportException("TransError");
        ret = e_ModuleName_Not_Found;
        break;
    }

    TLOGDEBUG("version: " << version << ", size: " << transferingInfoList.size() << endl);

    string addr;
    // 获取迁移源服务器的RouterClent对象名
    rc = getRouterClientJceAddr(packTable, transferInfo.groupName, addr);
    if (rc != e_Succ)
    {
        ret = rc;
        break;
    }

    try
    {
        RouterClientPrx routerClientPrx;
        if (_outerProxy->getProxy(addr, routerClientPrx) != 0)
        {
            ret = e_getProxy_Fail;
            break;
        }

        //异步方式调用
        pthread_mutex_t *mutex = NULL;
        pthread_cond_t *cond = NULL;
        if (_dbHandle->getTransferMutexCond(transferInfo.moduleName,
                                            TC_Common::tostr<pthread_t>(pthread_self()),
                                            &mutex,
                                            &cond) != 0)
        {
            ret = e_fromTransferStart_Fail;
            break;
        }
        routerClientPrx->tars_async_timeout(_transTo);
        TransferParamPtr param = new TransferParam(mutex, cond, e_fromTransfer_RestartSwitch_Fail);
        RouterClientPrxCallbackPtr cb = new RouterClientCallback(param);
        routerClientPrx->async_fromTransferStart(
            cb, transferInfo.moduleName, version, transferingInfoList, packTable);

        pthread_mutex_lock(mutex);
        pthread_cond_wait(cond, mutex);
        pthread_mutex_unlock(mutex);
        ret = param->_return;
    }
    catch (const TarsException &ex)
    {
        DAY_ERROR << "Transfer::notifyTransSrcServer catch exception: " << ex.what()
                  << " at: " << addr << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyTransSrcServer|") + ex.what() + " at: " + addr);
        _outerProxy->reportException("TransError");
        ret = e_fromTransferStart_Get_TarsException;
        _outerProxy->deleteProxy(addr);  // 清除代理缓存
    }

    PROC_END

    return ret;
}

int Transfer::notifyTransSrcServerDo(const TransferInfo &transferInfo)
{
    int ret = eSucc;
    PROC_BEGIN

    PackTable packTable;
    int rc = _dbHandle->getPackTable(transferInfo.moduleName, packTable);
    if (rc != 0)
    {
        DAY_ERROR << "Transfer::notifyTransSrcServerDo cant find moduleName: "
                  << transferInfo.moduleName << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyTransSrcServerDo|cant find moduleName: ") +
                         transferInfo.moduleName);
        _outerProxy->reportException("TransError");
        ret = e_ModuleName_Not_Found;
        break;
    }

    string addr;
    // 获取迁移源服务器的RouterClent对象名
    rc = getRouterClientJceAddr(packTable, transferInfo.groupName, addr);
    if (rc != e_Succ)
    {
        ret = rc;
        break;
    }

    try
    {
        RouterClientPrx routerClientPrx;
        if (_outerProxy->getProxy(addr, routerClientPrx) != 0)
        {
            ret = e_getProxy_Fail;
            break;
        }

        //异步方式调用
        pthread_mutex_t *mutex = NULL;
        pthread_cond_t *cond = NULL;
        if (_dbHandle->getTransferMutexCond(transferInfo.moduleName,
                                            TC_Common::tostr<pthread_t>(pthread_self()),
                                            &mutex,
                                            &cond) != 0)
        {
            ret = e_fromTransferDo_Fail;
            break;
        }
        routerClientPrx->tars_async_timeout(_transTo);
        TransferParamPtr param = new TransferParam(mutex, cond, e_fromTransfer_RestartSwitch_Fail);
        RouterClientPrxCallbackPtr cb = new RouterClientCallback(param);
        routerClientPrx->async_fromTransferDo(cb, transferInfo);

        pthread_mutex_lock(mutex);
        pthread_cond_wait(cond, mutex);
        pthread_mutex_unlock(mutex);
        ret = param->_return;
    }
    catch (const TarsException &ex)
    {
        DAY_ERROR << "Transfer::notifyTransSrcServerDo catch exception: " << ex.what()
                  << " at: " << addr << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyTransSrcServerDo|") + ex.what() + " at: " + addr);
        _outerProxy->reportException("TransError");
        ret = e_fromTransferDo_Get_TarsException;
        _outerProxy->deleteProxy(addr);  // 清除代理缓存
    }

    PROC_END

    return ret;
}

int Transfer::modifyRouterAfterTrans(TransferInfo *transInfoComplete)
{
    if (!transInfoComplete)
    {
        return e_Succ;
    }

    int ret = eSucc;
    PROC_BEGIN

    // 迁移成功，先修改数据库路由
    int rc = _dbHandle->changeDbRecord(*transInfoComplete);
    if (rc < 0)
    {
        DAY_ERROR << "Transfer::modifyRouterAfterTrans cant change record: "
                  << transInfoComplete->fromPageNo << ", " << transInfoComplete->toPageNo << endl;
        TARS_NOTIFY_WARN(string("Transfer::modifyRouterAfterTrans|cant change record: ") +
                         TC_Common::tostr<int>(transInfoComplete->fromPageNo) + ", " +
                         TC_Common::tostr<int>(transInfoComplete->toPageNo));
        _outerProxy->reportException("TransError");
        ret = e_Modify_DbRecord_Fail;

        break;
    }

    // 修改内存路由表
    rc = _dbHandle->modifyMemRecord(*transInfoComplete);
    if (rc < 0)
    {
        DAY_ERROR << "Transfer::modifyMemRecordAfterTrans cant modify record: "
                  << transInfoComplete->fromPageNo << ", " << transInfoComplete->toPageNo << endl;
        TARS_NOTIFY_WARN(string("Transfer::modifyMemRecordAfterTrans|cant modify record: ") +
                         TC_Common::tostr<int>(transInfoComplete->fromPageNo) + ", " +
                         TC_Common::tostr<int>(transInfoComplete->toPageNo));
        _outerProxy->reportException("TransError");
        ret = e_Modify_MemRecordAfterTrans_Fail;
        break;
    }

    // 更新迁移状态
    if (_dbHandle->setTransferStatus(*transInfoComplete, DbHandle::SET_TRANSFER_PAGE, "") < 0)
    {
        DAY_ERROR << "Transfer::modifyMemRecordAfterTrans setTransferStatus fail: "
                  << transInfoComplete->fromPageNo << ", " << transInfoComplete->toPageNo << endl;
        TARS_NOTIFY_WARN(string("Transfer::modifyMemRecordAfterTrans|setTransferStatus fail: ") +
                         TC_Common::tostr<int>(transInfoComplete->fromPageNo) + ", " +
                         TC_Common::tostr<int>(transInfoComplete->toPageNo));
        _outerProxy->reportException("TransError");
        ret = e_SetTransferedPage_Fail_Stop;
        break;
    }

    int retry = 0;
    do
    {
        if (deleteTransSrcServer(*transInfoComplete) != 0)
        {
            DAY_ERROR << "Transfer::modifyMemRecordAfterTrans deleteTransSrcServer fail: "
                      << transInfoComplete->fromPageNo << ", " << transInfoComplete->toPageNo
                      << endl;
            TLOGERROR("[Transfer::modifyMemRecordAfterTrans]" << endl);
            sleep(_retryTranInterval);
        }
        else
        {
            break;
        }
    } while (++retry < _retryTranMaxTimes);

    PROC_END

    return ret;
}

int Transfer::modifyRouterBefTrans(const TransferInfo &transferInfo)
{
    int ret = eSucc;
    PROC_BEGIN

    // 迁移前，先修改数据库路由
    int iRet = _dbHandle->changeDbRecord2(transferInfo);
    if (iRet < 0)
    {
        DAY_ERROR << "Transfer::modifyRouterBefTrans cant change record: "
                  << transferInfo.fromPageNo << ", " << transferInfo.toPageNo << endl;
        TARS_NOTIFY_WARN(string("Transfer::modifyRouterBefTrans|cant change record: ") +
                         TC_Common::tostr<int>(transferInfo.fromPageNo) + ", " +
                         TC_Common::tostr<int>(transferInfo.toPageNo));
        _outerProxy->reportException("TransError");
        ret = e_Modify_DbRecordBefTrans_Fail;

        break;
    }

    // 后修改内存路由表
    iRet = _dbHandle->modifyMemRecord2(transferInfo);
    if (iRet < 0)
    {
        DAY_ERROR << "Transfer::modifyMemRecordBefTrans cant modify record: "
                  << transferInfo.fromPageNo << ", " << transferInfo.toPageNo << endl;
        TARS_NOTIFY_WARN(string("Transfer::modifyMemRecordBefTrans|cant modify record: ") +
                         TC_Common::tostr<int>(transferInfo.fromPageNo) + ", " +
                         TC_Common::tostr<int>(transferInfo.toPageNo));
        _outerProxy->reportException("TransError");
        ret = e_Modify_MemRecordBefTrans_Fail;
        break;
    }

    PROC_END

    return ret;
}

int Transfer::notifyTransResult(const TransferInfo &transferInfo)
{
    int ret = eSucc;
    PROC_BEGIN

    PackTable packTable;
    vector<TransferInfo> transferingInfoList;
    int version;
    //取路由以及正处于迁移状态的记录，并通知迁移源及目的服务器
    int rc =
        _dbHandle->getRouterInfo(transferInfo.moduleName, version, transferingInfoList, packTable);
    if (rc != 0)
    {
        DAY_ERROR << "Transfer::notifyTransResult cant find moduleName: " << transferInfo.moduleName
                  << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyTransResult|cant find moduleName: ") +
                         transferInfo.moduleName);
        _outerProxy->reportException("TransError");
        ret = e_ModuleName_Not_Found;
        break;
    }

    // 通知迁移目的服务器
    rc = setServerRouterInfo(version, transferingInfoList, packTable, transferInfo.transGroupName);
    if (rc != e_Succ)
    {
        ret = e_NotifyDestSever_Result_Fail;
        break;
    }

    // 通知迁移源服务器
    rc = setServerRouterInfo(version, transferingInfoList, packTable, transferInfo.groupName);
    if (rc != e_Succ)
    {
        ret = e_NotifySrcSever_Result_Fail;
        break;
    }

    PROC_END

    return ret;
}

int Transfer::notifyTransServerClean(const string &moduleName, const string &groupName)
{
    int ret = eSucc;
    PROC_BEGIN

    PackTable packTable;
    //获取路由通知迁移源目的服务器
    int rc = _dbHandle->getPackTable(moduleName, packTable);
    if (rc != 0)
    {
        DAY_ERROR << "Transfer::notifyTransServerClean cant find moduleName: " << moduleName
                  << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyTransServerClean|cant find moduleName: ") +
                         moduleName);
        _outerProxy->reportException("TransError");
        ret = e_ModuleName_Not_Found;
        break;
    }

    string addr;
    // 获取迁移源服务器的RouterClent对象名
    rc = getRouterClientJceAddr(packTable, groupName, addr);
    if (rc != e_Succ)
    {
        ret = rc;
        break;
    }

    try
    {
        RouterClientPrx routerClientPrx;
        if (_outerProxy->getProxy(addr, routerClientPrx) != 0)
        {
            ret = e_getProxy_Fail;
            break;
        }
        vector<TransferInfo> v;
        rc = routerClientPrx->setTransRouterInfo(moduleName, TRANSFER_CLEAN_VERSION, v, packTable);
        if (rc == 0)
        {
            ret = e_Succ;
            break;
        }
        ret = e_SetRouterInfo_Fail;
    }
    catch (const TarsException &ex)
    {
        DAY_ERROR << "Transfer::notifyTransServerClean catch exception: " << ex.what()
                  << " at: " << addr << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyTransServerClean|") + ex.what() + " at: " + addr);
        _outerProxy->reportException("TransError");
        ret = e_SetRouterInfo_Get_TarsException;
        _outerProxy->deleteProxy(addr);  // 清除代理缓存
    }

    PROC_END

    return ret;
}

int Transfer::notifyAllServer(const string &moduleName,
                              vector<string> &succServers,
                              vector<string> &failServers)
{
    int ret = eSucc;
    PROC_BEGIN

    PackTable packTable;
    int rc = _dbHandle->getPackTable(moduleName, packTable);
    if (rc != 0)
    {
        DAY_ERROR << "Transfer::notifyAllServer cant find moduleName: " << moduleName << endl;
        TARS_NOTIFY_WARN(string("Transfer::notifyAllServer|cant find moduleName: ") + moduleName);
        _outerProxy->reportException("TransError");
        ret = e_ModuleName_Not_Found;
        break;
    }

    map<string, GroupInfo>::const_iterator it;
    for (it = packTable.groupList.begin(); it != packTable.groupList.end(); it++)
    {
        if (it->second.masterServer.empty()) continue;
        string addr = getRouterClientJceAddr(it->second.masterServer, packTable);
        try
        {
            RouterClientPrx routerClientPrx;
            if (_outerProxy->getProxy(addr, routerClientPrx) != 0)
                rc = -2;
            else
                rc = routerClientPrx->setRouterInfo(packTable.info.moduleName, packTable);
        }
        catch (const TarsException &ex)
        {
            DAY_ERROR << "Transfer::notifyAllServer catch exception: " << ex.what()
                      << " at: " << addr << endl;
            TARS_NOTIFY_WARN(string("Transfer::notifyAllServer|") + ex.what() + " at: " + addr);
            _outerProxy->reportException("TransError");
            rc = -1;
            _outerProxy->deleteProxy(addr);
        }

        if (rc != 0)
        {
            failServers.push_back(addr);
            DAY_ERROR << "Transfer::notifyAllServer notice server: " << it->second.masterServer
                      << " fail." << endl;
            TARS_NOTIFY_WARN(string("Transfer::notifyAllServer|notice server: ") +
                             it->second.masterServer);
            _outerProxy->reportException("TransError");
            if (rc == -2)
                rc = e_getProxy_Fail;
            else
                rc = e_SetRouterInfo_Fail;
        }
        else
        {
            succServers.push_back(addr);
        }
    }

    ret = rc;

    PROC_END

    return ret;
}

void Transfer::notifyAllServer(string moduleNames,
                               map<string, vector<string>> &succServers,
                               map<string, vector<string>> &failServers)
{
    map<string, ModuleInfo> infos;
    // 获取所有需要通知的模块名
    vector<string> names = SEPSTR(moduleNames, " ");
    moduleNames.clear();
    if (_dbHandle->getModuleInfos(infos) != 0) return;

    vector<string>::const_iterator vit = find(names.begin(), names.end(), "all");
    if (vit != names.end())
    {
        map<string, ModuleInfo>::const_iterator it;
        for (it = infos.begin(); it != infos.end(); it++)
        {
            if (notifyAllServer(it->first, succServers[it->first], failServers[it->first]) ==
                e_ModuleName_Not_Found)
            {
                moduleNames += it->first + " ";
            }
        }
    }
    else
    {
        map<string, ModuleInfo>::const_iterator it;
        for (unsigned i = 0; i < names.size(); i++)
        {
            it = infos.find(names[i]);
            if (it != infos.end())
            {
                if (notifyAllServer(it->first, succServers[it->first], failServers[it->first]) ==
                    e_ModuleName_Not_Found)
                {
                    moduleNames += it->first + " ";
                }
            }
            else
            {
                moduleNames += names[i] + " ";
            }
        }
    }

    return;
}

int Transfer::defragRouterInfo(const TransferInfo &transferInfo)
{
    int ret = eSucc;
    PROC_BEGIN

    // 先整理数据库中的
    int rc = _dbHandle->defragDbRecord(transferInfo);
    if (rc < 0)
    {
        DAY_ERROR << "Transfer::defragRouterInfo cant defrag db record: " << transferInfo.fromPageNo
                  << ", " << transferInfo.toPageNo << endl;
        TARS_NOTIFY_WARN(string("Transfer::defragRouterInfo|cant defrag db record: ") +
                         TC_Common::tostr<int>(transferInfo.fromPageNo) + ", " +
                         TC_Common::tostr<int>(transferInfo.toPageNo));
        _outerProxy->reportException("TransError");
        ret = e_DefragDbRecord_Fail;
        break;
    }

    // 是重新从数据库中加载路由还是自行更新内存中的路由呢？？？？
    // 先尝试自行整理
    // 再整理内存中的
    rc = _dbHandle->defragMemRecord(transferInfo);
    if (rc < 0)
    {
        DAY_ERROR << "Transfer::defragRouterInfo cant defrag mem record: "
                  << transferInfo.fromPageNo << ", " << transferInfo.toPageNo << endl;
        TARS_NOTIFY_WARN(string("Transfer::defragRouterInfo|cant defrag mem record: ") +
                         TC_Common::tostr<int>(transferInfo.fromPageNo) + ", " +
                         TC_Common::tostr<int>(transferInfo.toPageNo));
        _outerProxy->reportException("TransError");
        ret = e_DefragMemRecord_Fail;
        break;
    }

    PROC_END

    return ret;
}

int Transfer::canTransfer(TransferInfo &transferInfo)
{
    int ret = eSucc;

    bool hasAddedToMem = false;

    PROC_BEGIN

    PackTable packTable;
    // 是否能获取到业务模块
    int iRet = _dbHandle->getPackTable(transferInfo.moduleName, packTable);
    if (iRet != 0)
    {
        ret = e_Transfer_ModuleName_Not_Found;
        break;
    }

    // 页范围是否正确
    if (transferInfo.fromPageNo > transferInfo.toPageNo || transferInfo.fromPageNo < 0 ||
        transferInfo.toPageNo < 0 || transferInfo.toPageNo > (int)__ALL_PAGE_COUNT)
    {
        ret = e_Transfer_Page_Incorrect;
        break;
    }

    // 服务器组名是否为空
    if (transferInfo.groupName.empty() || transferInfo.transGroupName.empty())
    {
        ret = e_Transfer_GroupName_Incorrect;
        break;
    }

    // 源服务器组名是否存在
    map<string, GroupInfo>::const_iterator itGroup =
        packTable.groupList.find(transferInfo.groupName);
    if (itGroup == packTable.groupList.end())
    {
        ret = e_Transfer_GroupName_Not_Found;
        break;
    }

    // 目的服务器组名是否存在
    map<string, GroupInfo>::const_iterator itTransGroup =
        packTable.groupList.find(transferInfo.transGroupName);
    if (itTransGroup == packTable.groupList.end())
    {
        ret = e_Transfer_TransGroupName_Not_Found;
        break;
    }

    // 源服务器组名与目的服务器组名是否相同
    if (transferInfo.groupName == transferInfo.transGroupName)
    {
        ret = e_Transfer_GroupName_Equal;
        break;
    }

    // 源服务器服务名是否存在
    if (itGroup->second.masterServer.empty())
    {
        ret = e_Transfer_ServerName_Not_Found;
        break;
    }

    // 源服务器信息是否存在
    map<string, ServerInfo>::const_iterator itServer =
        packTable.serverList.find(itGroup->second.masterServer);
    if (itServer == packTable.serverList.end())
    {
        ret = e_Transfer_ServerInfo_Not_Found;
        break;
    }
    else
    {
        // 源服务器是否真正主机
        if (itServer->second.ServerStatus != "M")
        {
            ret = e_Transfer_Server_ServerStatus_Not_M;
            break;
        }
    }

    // 目的服务器服务名是否存在
    if (itTransGroup->second.masterServer.empty())
    {
        ret = e_Transfer_TransServerName_Not_Found;
        break;
    }

    // 目的服务器信息是否存在
    map<string, ServerInfo>::const_iterator itTransServer =
        packTable.serverList.find(itTransGroup->second.masterServer);
    if (itTransServer == packTable.serverList.end())
    {
        ret = e_Transfer_TransServerInfo_Not_Found;
        break;
    }
    else
    {
        // 目的服务器是否真正主机
        if (itTransServer->second.ServerStatus != "M")
        {
            ret = e_Transfer_TransServer_ServerStatus_Not_M;
            break;
        }
    }

    // 模块当前是否允许加入该迁移记录
    if (_dbHandle->addMemTransferRecord(transferInfo) < 0)
    {
        ret = e_Transfer_Module_Conflict;
        break;
    }

    //记录本次迁移任务所使用的锁和条件变量
    if (_dbHandle->addMemTransferMutexCond(transferInfo) < 0)
    {
        ret = e_Transfer_Module_Conflict;
        break;
    }

    hasAddedToMem = true;

    // 所有页是否可以迁移
    iRet = canTransfer(transferInfo, packTable);
    if (iRet != e_Succ)
    {
        ret = iRet;
        break;
    }

    if (transferInfo.id > 0)
    {
        // 设置迁移记录为正在迁移状态
        if (_dbHandle->setStartTransferStatus(transferInfo) < 0)
        {
            ret = e_SetTransfering_Fail;
            break;
        }
    }
    else
    {
        // 将迁移记录写入数据库中
        if (_dbHandle->writeDbTransferRecord(transferInfo) < 0)
        {
            ret = e_writeDbTransferRecord_Fail;
            break;
        }
    }

    PROC_END

    if (ret != e_Succ && hasAddedToMem)
    {
        bool b;
        removeTransfer(transferInfo, b);
    }

    return ret;
}

int Transfer::canTransfer(const TransferInfo &transferInfo, const PackTable &packTable)
{
    for (int i = transferInfo.fromPageNo; i <= transferInfo.toPageNo; ++i)
    {
        bool b = false;
        int j = 0;
        for (size_t k = 0; k < packTable.recordList.size(); ++k)
        {
            if (i >= packTable.recordList[k].fromPageNo && i <= packTable.recordList[k].toPageNo)
            {
                if (j == 0)
                {
                    ++j;
                    if (transferInfo.groupName == packTable.recordList[k].groupName)
                    {
                        b = true;
                    }
                }
                else
                {
                    return e_TransferPage_Route_Incorrect;
                }
            }
        }

        if (!b)
        {
            return e_PageNo_NotOwnedBySrcGroupName;
        }
    }

    return e_Succ;
}

bool Transfer::isOwnedbySrcGroup(const TransferInfo &transferInfo, const PackTable &packTable)
{
    for (int i = transferInfo.fromPageNo; i <= transferInfo.toPageNo; i++)
    {
        bool b = false;  // 默认当前页不属于源服务器
        for (size_t j = 0; j < packTable.recordList.size(); j++)
        {
            if (i >= packTable.recordList[j].fromPageNo && i <= packTable.recordList[j].toPageNo &&
                transferInfo.groupName == packTable.recordList[j].groupName)
            {    
                b = true;  // 当前页属于源服务器
            }
        }

        // 如果当前页不属于源服务器，立即返回
        if (!b)
        {
            return false;
        }
    }

    return true;
}

int Transfer::removeTransfer(const TransferInfo &transferInfo, bool &bModuleComplete)
{
    int ret = eSucc;
    PROC_BEGIN

    // 清除内存中已迁移记录
    int rc = _dbHandle->removeMemTransferRecord(transferInfo, bModuleComplete);
    if (rc < 0)
    {
        DAY_ERROR << "Transfer::removeTransfer cant remove mem transfer record: "
                  << transferInfo.fromPageNo << ", " << transferInfo.toPageNo << endl;
        TARS_NOTIFY_WARN(string("Transfer::removeTransfer|cant remove mem transfer record: ") +
                         TC_Common::tostr<int>(transferInfo.fromPageNo) + ", " +
                         TC_Common::tostr<int>(transferInfo.toPageNo));
        _outerProxy->reportException("TransError");
        ret = e_removeMemTransferRecord_Fail;
        break;
    }

    rc = _dbHandle->removeMemTransferMutexCond(transferInfo);
    if (rc < 0)
    {
        DAY_ERROR << "Transfer::removeTransfer cant remove mem transfer mutex and cond: "
                  << transferInfo.fromPageNo << ", " << transferInfo.toPageNo << endl;
        TARS_NOTIFY_WARN(
            string("Transfer::removeTransfer|cant remove mem transfer mutex and cond: ") +
            TC_Common::tostr<int>(transferInfo.fromPageNo) + ", " +
            TC_Common::tostr<int>(transferInfo.toPageNo));
        _outerProxy->reportException("TransError");
        ret = e_removeMemTransferRecord_Fail;
        break;
    }

    PROC_END

    return ret;
}

int Transfer::getRouterClientJceAddr(PackTable &packTable, const string &groupName, string &addr)

{
    if (packTable.groupList[groupName].groupName.empty())
    {
        DAY_ERROR << "Transfer::getRouterClientJceAddr cant find groupName: " << groupName << endl;
        TARS_NOTIFY_WARN(string("Transfer::getRouterClientJceAddr|cant find groupName: ") +
                         groupName);
        _outerProxy->reportException("TransError");
        return e_GroupName_Not_Found;
    }

    if (packTable.groupList[groupName].masterServer.empty())
    {
        DAY_ERROR << "Transfer::getRouterClientJceAddr cant find masterServer of groupName: "
                  << groupName << endl;
        TARS_NOTIFY_WARN(
            string("Transfer::getRouterClientJceAddr|cant find masterServer of groupName: ") +
            groupName);
        _outerProxy->reportException("TransError");
        return e_ServerName_Not_Found;
    }

    map<string, ServerInfo>::const_iterator it =
        packTable.serverList.find(packTable.groupList[groupName].masterServer);
    if (it == packTable.serverList.end())
    {
        DAY_ERROR << "Transfer::getRouterClientJceAddr cant find ServerInfo of groupName: "
                  << groupName << " masterServer: " << packTable.groupList[groupName].masterServer
                  << endl;
        TARS_NOTIFY_WARN(
            string("Transfer::getRouterClientJceAddr|cant find ServerInfo of groupName: ") +
            groupName + string(" masterServer: ") + packTable.groupList[groupName].masterServer);
        _outerProxy->reportException("TransError");
        return e_ServerInfo_Not_Found;
    }

    if (it->second.ServerStatus != "M")
    {
        DAY_ERROR << "Transfer::getRouterClientJceAddr masterServer: "
                  << packTable.groupList[groupName].masterServer << " of groupName: " << groupName
                  << " 's ServerStatus!=M" << endl;
        TARS_NOTIFY_WARN(string("Transfer::getRouterClientJceAddr|masterServer: ") +
                         packTable.groupList[groupName].masterServer + string(" of groupName: ") +
                         groupName + string(" 's ServerStatus!=M"));
        _outerProxy->reportException("TransError");
        return e_ServerStatus_Not_M;
    }

    addr = it->second.RouteClientServant;
    return e_Succ;
}

string Transfer::getRouterClientJceAddr(const string &server, PackTable &packTable)
{
    return packTable.serverList[server].RouteClientServant;
}

int Transfer::setServerRouterInfo(int iTransInfoListVer,
                                  const vector<TransferInfo> &transferingInfoList,
                                  PackTable &packTable,
                                  const string &groupName)
{
    string addr;
    int iRet = getRouterClientJceAddr(packTable, groupName, addr);

    if (iRet != e_Succ)
    {
        return iRet;
    }

    try
    {
        RouterClientPrx routerClientPrx;
        if (_outerProxy->getProxy(addr, routerClientPrx) != 0)
        {
            return e_getProxy_Fail;
        }

        iRet = routerClientPrx->setTransRouterInfo(
            packTable.info.moduleName, iTransInfoListVer, transferingInfoList, packTable);
        if (iRet == 0)
        {
            return e_Succ;
        }
    }
    catch (TarsException &ex)
    {
        DAY_ERROR << "Transfer::setServerRouterInfo "
                     "catch exception: "
                  << ex.what() << " at: " << addr << endl;
        TARS_NOTIFY_WARN(string("Transfer::setServerRouterInfo|") + ex.what() + " at: " + addr);
        _outerProxy->reportException("TransError");
        _outerProxy->deleteProxy(addr);
    }

    return e_SetRouterInfo_Fail;
}

int Transfer::setServerRouterInfo(PackTable &packTable, const string &groupName)
{
    string addr;
    int rc = getRouterClientJceAddr(packTable, groupName, addr);
    if (rc != e_Succ) return rc;

    try
    {
        RouterClientPrx routerClientPrx;
        if (_outerProxy->getProxy(addr, routerClientPrx) != 0) return e_getProxy_Fail;
        rc = routerClientPrx->setRouterInfo(packTable.info.moduleName, packTable);
        if (rc == 0) return e_Succ;
    }
    catch (const TarsException &ex)
    {
        DAY_ERROR << "Transfer::setServerRouterInfo "
                     "catch exception: "
                  << ex.what() << " at: " << addr << endl;
        TARS_NOTIFY_WARN(string("Transfer::setServerRouterInfo|") + ex.what() + " at: " + addr);
        _outerProxy->reportException("TransError");
        _outerProxy->deleteProxy(addr);
    }

    return e_SetRouterInfo_Fail;
}

int Transfer::reloadRouter()
{
    int ret = eSucc;
    PROC_BEGIN

    //是不是应该限制只加载指定模块的？？？？
    int rc = _dbHandle->reloadRouter();
    if (rc == 1)
    {
        ret = e_NotReloadRouter_Transfering;
        break;
    }
    else if (rc == -1)
    {
        ret = e_ReloadRouter_Fail;
        break;
    }

    PROC_END

    return ret;
}

int Transfer::SetTransferEnd(const TransferInfo &transferInfo, const string &info)
{
    int ret = eSucc;
    PROC_BEGIN

    // 设置迁移结束
    if (_dbHandle->setTransferStatus(transferInfo, DbHandle::TRANSFERED, info) < 0)
    {
        ret = e_SetTransfered_Fail;
        break;
    }

    PROC_END

    return ret;
}

int Transfer::doTransfer(const TransferInfo &transferInfoIn, string &info)
{
    TransferInfo transferInfo = transferInfoIn;

    int ret = e_Unknown;
    int tryTimes = 0;
    bool bCanRemoveTransInfo = true;
    int transferPageOnceTmp = _transferPagesOnce;
    DAY_ERROR << "Transfer::doTransfer  set transferPageOnceTmp=" << transferPageOnceTmp << endl;
    PROC_BEGIN

    vector<int> retValList;
    info.clear();

    // 判断是否可以迁移
    ret = canTransfer(transferInfo);
    if (ret != e_Succ)
    {
        retValList.push_back(ret);
        info = RetVal2Info(retValList);
        transferInfo.toPageNo = -1;
        SetTransferEnd(transferInfo, info);  // 不能迁移，修改迁移记录状态
        break;
    }

    //迁移前修改路由信息，使迁移范围内页码在一条记录内
    ret = modifyRouterBefTrans(transferInfo);
    if (ret != e_Succ)
    {
        retValList.push_back(ret);
        bool bComplete;
        removeTransfer(transferInfo, bComplete);
        info = RetVal2Info(retValList);
        transferInfo.toPageNo = -1;
        SetTransferEnd(transferInfo, info);  // 不能迁移，修改迁移记录状态
        break;
    }

    // 按页迁移
    TransferInfo *transInfoNew = new TransferInfo(transferInfo);
    TransferInfo *transInfoOld = NULL;
    int lastPage = transferInfo.fromPageNo - 1, lastSuccPage = lastPage, times = -1;
    do
    {
        int rc = modifyRouterAfterTrans(transInfoOld);
        if (rc != e_Succ)
        {
            if (rc == e_SetTransferedPage_Fail_Stop)
            {
                //修改路由失败后更新内存中正处于迁移状态的记录
                int rc1 = updateTransferingRecord(NULL, transInfoOld);
                if (rc1 != e_Succ)
                {
                    ret = rc1;
                    retValList.push_back(ret);
                }

                rc1 = notifyTransResult(transferInfo);
                if (rc1 != e_Succ)
                {
                    ret = rc1;
                    retValList.push_back(ret);
                }
                lastSuccPage = lastPage;
                ++times;
            }
            else
            {
                bCanRemoveTransInfo = false;
            }
            ret = rc;
            retValList.push_back(ret);

            break;
        }

        lastSuccPage = lastPage;
        ++times;
        //如果已经进行了多次迁移，则对数据库以及内存中的路由信息进行整理
        if (times * transferPageOnceTmp >= _defragDbInterval)
        {
            times = 0;
            TransferInfo tmpTransInfo = transferInfo;
            tmpTransInfo.toPageNo = lastSuccPage;
            // tmpTransInfo.toPageNo = iFromPageNo-1;
            if (tmpTransInfo.toPageNo > tmpTransInfo.fromPageNo)
            {
                //整理
                rc = defragRouterInfo(tmpTransInfo);
                if (rc != e_Succ)
                {
                    ret = rc;
                    retValList.push_back(ret);
                    break;
                }
            }
        }

        transInfoNew->fromPageNo = lastPage + 1;
        transInfoNew->toPageNo = (lastPage + transferPageOnceTmp > transferInfo.toPageNo)
                                     ? transferInfo.toPageNo
                                     : lastPage + transferPageOnceTmp;
        //根据迁移信息修改内存中正处于迁移状态的记录
        rc = updateTransferingRecord(transInfoNew, transInfoOld);
        if (rc != e_Succ)
        {
            ret = rc;
            retValList.push_back(ret);

            rc = notifyTransResult(transferInfo);
            if (rc != e_Succ)
            {
                ret = rc;
                retValList.push_back(ret);
            }
            break;
        }

        tryTimes = 0;
        do
        {
            rc = notifyTransDestServer(*transInfoNew);
            if (rc == e_Succ)
            {
                break;
            }
            else
            {
                sleep(_retryTranInterval);
            }
        } while (++tryTimes < _retryTranMaxTimes);
        if (tryTimes >= _retryTranMaxTimes)
        {
            ret = rc;
            retValList.push_back(ret);
            rc = updateTransferingRecord(NULL, transInfoNew);
            if (rc != e_Succ)
            {
                retValList.push_back(rc);
            }
            rc = notifyTransResult(transferInfo);
            if (rc != e_Succ)
            {
                ret = rc;
                retValList.push_back(ret);
            }
            break;
        }

        tryTimes = 0;
        do
        {
            rc = notifyTransSrcServer(*transInfoNew);
            if (rc == e_Succ)
            {
                break;
            }
            else
            {
                sleep(_retryTranInterval);
            }
        } while (++tryTimes < _retryTranMaxTimes);
        if (tryTimes == _retryTranMaxTimes)
        {
            ret = rc;
            retValList.push_back(ret);
            rc = updateTransferingRecord(NULL, transInfoNew);
            if (rc != e_Succ)
            {
                retValList.push_back(rc);
            }
            rc = notifyTransResult(transferInfo);
            if (rc != e_Succ)
            {
                ret = rc;
                retValList.push_back(ret);
            }
            break;
        }

        tryTimes = 0;
        do
        {
            rc = notifyTransSrcServerDo(*transInfoNew);
            if (rc == e_Succ)
            {
                break;
            }
            else if (rc == e_fromTransfer_RestartSwitch_Fail)
            {
                int retry = 0;
                do
                {
                    //源组可能重启或者切换，内存中数据丢失，重试时，需要重新获取迁移记录列表
                    rc = notifyTransSrcServer(*transInfoNew);
                    if (rc != e_Succ)
                    {
                        sleep(_retryTranInterval);
                    }
                    else
                    {
                        break;
                    }
                } while (++retry < _retryTranMaxTimes);
            }
            else
            {
                //目的组可能重启或者切换，需要等待重启或者切换完成，或者写数据到目的组失败，也需要等待
                sleep(180);
            }
        } while (++tryTimes < _retryTranMaxTimes);
        if (tryTimes == _retryTranMaxTimes)
        {
            ret = rc;
            retValList.push_back(ret);
            rc = updateTransferingRecord(NULL, transInfoNew);
            if (rc != e_Succ)
            {
                retValList.push_back(rc);
            }
            rc = notifyTransResult(transferInfo);
            if (rc != e_Succ)
            {
                ret = rc;
                retValList.push_back(ret);
            }
            //迁移三次都超时后，把迁移页数除以2，由opt的迁移的在下次重试迁移时生效
            {
                TC_ThreadLock::Lock lock(_transferPageLock);
                if (transferPageOnceTmp <= _transferPagesOnce)
                {
                    _transferPagesOnce = transferPageOnceTmp / 2 > 1 ? transferPageOnceTmp / 2 : 1;
                }
                DAY_ERROR << "Transfer::notifyTransSrcServerDo tryTime>3 set transferPagesOnce="
                          << _transferPagesOnce << endl;
            }
            break;
        }

        if (!transInfoOld)
        {
            transInfoOld = new TransferInfo(*transInfoNew);
        }
        TransferInfo *pTemp = transInfoOld;
        transInfoOld = transInfoNew;
        transInfoNew = pTemp;
        lastPage = transInfoOld->toPageNo;
        // iFromPageNo = transInfoOld->toPageNo + 1;
    } while (lastPage < transferInfo.toPageNo);  // while(iFromPageNo<=transferInfo.toPageNo);

    TransferInfo tmpTransInfo = transferInfo;
    tmpTransInfo.fromPageNo = transferInfo.fromPageNo;
    tmpTransInfo.toPageNo = lastSuccPage >= transferInfo.fromPageNo ? lastSuccPage : -1;
    // 如果成功迁移完所有页后执行以下操作
    if (lastPage == transferInfo.toPageNo)
    {
        int rc = modifyRouterAfterTrans(transInfoOld);

        if (rc == e_Succ || rc == e_SetTransferedPage_Fail_Stop)
        {
            tmpTransInfo.toPageNo = lastPage;
            // lastSuccPage = lastPage;
            ++times;

            int rc1 = updateTransferingRecord(NULL, transInfoOld);
            if (rc1 != e_Succ)
            {
                ret = rc;
                retValList.push_back(ret);
            }

            rc1 = notifyTransResult(transferInfo);
            if (rc1 != e_Succ)
            {
                ret = rc1;
                retValList.push_back(ret);
            }
        }
        else
        {
            bCanRemoveTransInfo = false;
        }

        if (rc != e_Succ)
        {
            ret = rc;
            retValList.push_back(ret);
        }
    }

    //如果成功迁移多次，则对数据库以及内存中的路由信息进行整理
    if (tmpTransInfo.toPageNo >= tmpTransInfo.fromPageNo + transferPageOnceTmp && times > 0)
    {
        //整理
        int rc = defragRouterInfo(tmpTransInfo);
        if (rc != e_Succ)
        {
            ret = rc;
            retValList.push_back(ret);
        }
        else
        {
            rc = notifyTransResult(tmpTransInfo);
            if (rc != e_Succ)
            {
                ret = rc;
                retValList.push_back(ret);
            }
        }
    }

    if (bCanRemoveTransInfo)
    {
        //迁移结束时要清除内存迁移记录
        bool bModuleComplete = false;
        int rc = removeTransfer(transferInfo, bModuleComplete);
        if (rc != e_Succ)
        {
            ret = rc;
            retValList.push_back(ret);
        }

        if (bModuleComplete)
        {
            // 当前模块迁移完全结束，重新加载路由
            rc = reloadRouter();
            if (rc != e_Succ)
            {
                TLOGDEBUG("reloadRouter fail:other transfers is running" << endl);
            }  //为了防止多模块同时迁移 导致的迁移失败 取消检查
        }
    }
    // 成功
    if (ret == e_Succ)
    {
        retValList.push_back(ret);
    }
    // 设置迁移结束状态
    info = RetVal2Info(retValList);
    SetTransferEnd(tmpTransInfo, info);

    // 迁移成功或失败都上报
    if (ret == e_Succ)
    {
        //迁移成功后，把迁移页数恢复成最大值
        {
            TC_ThreadLock::Lock lock(_transferPageLock);
            if (transferPageOnceTmp != _transferPagesOnceMax &&
                transferPageOnceTmp <= _transferPagesOnce)
            {
                _transferPagesOnce = _transferPagesOnceMax;
                DAY_ERROR << "Transfer::doTransfer succ set transferPagesOnce="
                          << _transferPagesOnceMax << endl;
            }
        }
        TARS_NOTIFY_NORMAL(string("Transfer::doTransfer|Succ"));
    }
    else
    {
        TARS_NOTIFY_WARN(string("RouterImp::transfer|") + info);
        _outerProxy->reportException("TransError");
    }

    delete transInfoNew;
    delete transInfoOld;

    PROC_END

    return ret;
}

string Transfer::RetVal2Info(const vector<int> &retValList)
{
    ostringstream os;
    for (size_t i = 0; i < retValList.size(); i++)
    {
        os << RT2STR(retValList[i]);
        if (i != retValList.size() - 1)
        {
            os << endl;
        }
    }

    return os.str();
}

int Transfer::deleteTransSrcServer(const TransferInfo &transferInfo)
{
    int ret = eSucc;
    PROC_BEGIN

    PackTable packTable;
    vector<TransferInfo> transferingInfoList;
    int version;

    int rc =
        _dbHandle->getRouterInfo(transferInfo.moduleName, version, transferingInfoList, packTable);
    if (rc != 0)
    {
        DAY_ERROR << "Transfer::deleteTransSrcServer cant find moduleName: "
                  << transferInfo.moduleName << endl;
        TARS_NOTIFY_WARN(string("Transfer::deleteTransSrcServer|cant find moduleName: ") +
                         transferInfo.moduleName);
        _outerProxy->reportException("TransError");
        ret = e_ModuleName_Not_Found;
        break;
    }
    TLOGDEBUG("version: " << version << ", size: " << transferingInfoList.size() << endl);

    string addr;
    // 获取迁移源服务器的RouterClent对象名
    rc = getRouterClientJceAddr(packTable, transferInfo.groupName, addr);
    if (rc != e_Succ)
    {
        ret = rc;
        break;
    }

    try
    {
        RouterClientPrx routerClientPrx;
        if (_outerProxy->getProxy(addr, routerClientPrx) != 0)
        {
            ret = e_getProxy_Fail;
            break;
        }

        //异步方式调用
        pthread_mutex_t *mutex = NULL;
        pthread_cond_t *cond = NULL;
        if (_dbHandle->getTransferMutexCond(transferInfo.moduleName,
                                            TC_Common::tostr<pthread_t>(pthread_self()),
                                            &mutex,
                                            &cond) != 0)
        {
            ret = e_CleanFromTransferData_Fail;
            break;
        }
        routerClientPrx->tars_async_timeout(_transTo);
        TransferParamPtr param = new TransferParam(mutex, cond, e_fromTransfer_RestartSwitch_Fail);
        RouterClientPrxCallbackPtr cb = new RouterClientCallback(param);
        routerClientPrx->async_cleanFromTransferData(
            cb, transferInfo.moduleName, transferInfo.fromPageNo, transferInfo.toPageNo);

        pthread_mutex_lock(mutex);
        pthread_cond_wait(cond, mutex);
        pthread_mutex_unlock(mutex);

        ret = param->_return;
    }
    catch (const TarsException &ex)
    {
        DAY_ERROR << "Transfer::deleteTransSrcServer catch exception: " << ex.what()
                  << " at: " << addr << endl;
        TARS_NOTIFY_WARN(string("Transfer::deleteTransSrcServer|") + ex.what() + " at: " + addr);
        _outerProxy->reportException("TransError");
        ret = e_toTransferStart_Get_TarsException;
        _outerProxy->deleteProxy(addr);  // 清除代理缓存
    }

    PROC_END

    return ret;
}

void RouterClientCallback::callback_toTransferStart(tars ::Int32 ret)
{
    if (ret == 0)
    {
        _param->_return = eSucc;
    }
    else
    {
        _param->_return = e_toTransferStart_Fail;
    }
    //唤醒等待的线程
    wakeUpWaitingThread();
}

void RouterClientCallback::callback_toTransferStart_exception(tars ::Int32 ret)
{
    _param->_return = e_toTransferStart_Get_TarsException;

    //唤醒等待的线程
    wakeUpWaitingThread();
}

void RouterClientCallback::callback_fromTransferStart(tars ::Int32 ret)
{
    if (ret == 0)
    {
        _param->_return = eSucc;
    }
    else
    {
        _param->_return = e_fromTransferStart_Fail;
    }
    //唤醒等待的线程
    wakeUpWaitingThread();
}

void RouterClientCallback::callback_fromTransferStart_exception(tars ::Int32 ret)
{
    _param->_return = e_fromTransferStart_Get_TarsException;

    //唤醒等待的线程
    wakeUpWaitingThread();
}

void RouterClientCallback::callback_fromTransferDo(tars ::Int32 ret)
{
    if (ret == 0)
    {
        _param->_return = eSucc;
    }
    else
    {
        _param->_return = e_fromTransferDo_Fail;
    }
    //唤醒等待的线程
    wakeUpWaitingThread();
}

void RouterClientCallback::callback_fromTransferDo_exception(tars ::Int32 ret)
{
    _param->_return = e_fromTransferDo_Get_TarsException;

    //唤醒等待的线程
    wakeUpWaitingThread();
}

void RouterClientCallback::callback_cleanFromTransferData(tars ::Int32 ret)
{
    if (ret == 0)
    {
        _param->_return = eSucc;
    }
    else
    {
        _param->_return = e_CleanFromTransferData_Fail;
    }
    //唤醒等待的线程
    wakeUpWaitingThread();
}

void RouterClientCallback::callback_cleanFromTransferData_exception(tars ::Int32 ret)
{
    _param->_return = e_CleanFromTransferData_Get_TarsException;

    //唤醒等待的线程
    wakeUpWaitingThread();
}

TransferDispatcher::TransferDispatcher(DoTransfer func,
                                       int threadPoolSize,
                                       int minSizeEachGroup,
                                       int maxSizeEachGroup)
    : _tasksNumInQueue(0),
      _TransferingTasksNum(0),
      _idleThreadNum(threadPoolSize),
      _transferFunc(func),
      _minThreadEachGroup(minSizeEachGroup),
      _maxThreadEachGroup(maxSizeEachGroup)
{
    _tpool.init(_idleThreadNum);
    _tpool.start();
}

void TransferDispatcher::addTransferTask(std::shared_ptr<TransferInfo> task)
{
    auto it = _taskQueue.find(task->groupName);
    if (it == _taskQueue.end())
    {
        it = _taskQueue.emplace(task->groupName, std::queue<std::shared_ptr<TransferInfo>>()).first;
    }
    it->second.push(task);
    ++_tasksNumInQueue;
    FDLOG("TransferDispatcher") << __LINE__ << "|" << __FUNCTION__ << "|"
                                << "moduleName: " << task->moduleName << "|"
                                << "total tasks: " << it->second.size() << endl;
}

void TransferDispatcher::transferFuncWrapper(std::shared_ptr<TransferInfo> task)
{
    std::string transferResult;
    (void)_transferFunc(*task, transferResult);
    {
        TC_ThreadLock::Lock lock(_transferingTasksLock);
        auto it = _transferingTasks.find(task->groupName);
        assert(it != _transferingTasks.end());
        --(it->second);
        --_TransferingTasksNum;
        if (it->second == 0)
        {
            _transferingTasks.erase(it);
        }
    }
    ++_idleThreadNum;
}

void TransferDispatcher::dispatcherTask(int maxThreadNum)
{
    TC_ThreadLock::Lock lock(_transferingTasksLock);
    auto remaningThreadNum = maxThreadNum * _taskQueue.size() - _transferingTasks.size();
    // 如果有空闲线程和待执行任务，且存在任务，其分配的任务数没有达到指定的最大值
    while (_idleThreadNum > 0 && _tasksNumInQueue > 0 && remaningThreadNum > 0)
    {
        bool endDispatch = true;
        for (auto it = _taskQueue.begin(); it != _taskQueue.end() && _idleThreadNum > 0;)
        {
            auto t = _transferingTasks.find(it->first);
            if (t == _transferingTasks.end() || t->second < maxThreadNum)
            {
                assert(it->second.size() > 0);
                std::shared_ptr<TransferInfo> task = it->second.front();
                _tpool.exec(
                    std::bind(&TransferDispatcher::transferFuncWrapper, shared_from_this(), task));
                it->second.pop();
                --_tasksNumInQueue;
                ++_TransferingTasksNum;
                --_idleThreadNum;
                ++_transferingTasks[it->first];
                --remaningThreadNum;
                TLOGDEBUG("Transfer group name : " << it->first << " | processing thread num: "
                                                   << _transferingTasks[it->first]
                                                   << " | idle thread num : " << _idleThreadNum
                                                   << '\n');
                if (it->second.empty())
                {
                    it = _taskQueue.erase(it);
                }
                else
                {
                    ++it;
                }
                endDispatch = false;
            }
            else
            {
                ++it;
            }
        }
        if (endDispatch)
        {
            break;
        }
    }
}

void TransferDispatcher::doTransferTask()
{
    if (_taskQueue.empty() || _idleThreadNum <= 0) return;
    // 为每个组分配最小数目的线程
    dispatcherTask(_minThreadEachGroup);
    // 如果还有空闲线程，就再分配给剩余的任务
    if (_taskQueue.empty() || _idleThreadNum <= 0) return;
    dispatcherTask(_maxThreadEachGroup);
}

void TransferDispatcher::clearTasks()
{
    {
        _taskQueue.clear();
    }
    _tasksNumInQueue = 0;
}

void TransferDispatcher::terminate()
{
    TLOGDEBUG("waitForAllDone..." << endl);
    //停止线程
    _tpool.stop();
    bool b = _tpool.waitForAllDone(3000);  // 先等待三秒
    TLOGDEBUG("waitForAllDone..." << b << ":" << _tpool.getJobNum() << endl);

    if (!b)  // 没停止则再永久等待
    {
        TLOGDEBUG("waitForAllDone again, but forever..." << endl);
        b = _tpool.waitForAllDone(-1);
        TLOGDEBUG("waitForAllDone again..." << b << ":" << _tpool.getJobNum() << endl);
    }
}

int TransferDispatcher::getQueueTaskNum(const std::string &groupName) const
{
    TransferQueue::const_iterator it = _taskQueue.find(groupName);
    if (it == _taskQueue.cend())
    {
        return 0;
    }
    else
    {
        return it->second.size();
    }
}

int TransferDispatcher::getTransferingTaskNum(const std::string &groupName) const
{
    TC_ThreadLock::Lock lock(_transferingTasksLock);
    std::unordered_map<std::string, int>::const_iterator it = _transferingTasks.find(groupName);
    if (it == _transferingTasks.cend())
    {
        return 0;
    }
    else
    {
        return it->second;
    }
}

}  // namespace DCache

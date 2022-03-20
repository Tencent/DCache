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
#include "CacheGlobe.h"

namespace DCache
{

int PPReport::init()
{
    _srp_ex = Application::getCommunicator()->getStatReport()->createPropertyReport("ProgramException", PropertyReport::sum());
    if (_srp_ex == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_ex is NULL." << endl);
        return -1;
    }
    _srp_binlogerr = Application::getCommunicator()->getStatReport()->createPropertyReport("M/S_ReplicationErr", PropertyReport::sum());
    if (_srp_binlogerr == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_binlogerr is NULL." << endl);
        return -1;
    }
    _srp_dbex = Application::getCommunicator()->getStatReport()->createPropertyReport("DbException", PropertyReport::sum());
    if (_srp_dbex == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_dbex is NULL." << endl);
        return -1;
    }
    _srp_dberr = Application::getCommunicator()->getStatReport()->createPropertyReport("DbError", PropertyReport::sum());
    if (_srp_dberr == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_dberr is NULL." << endl);
        return -1;
    }
    _srp_cache_err = Application::getCommunicator()->getStatReport()->createPropertyReport("CacheError", PropertyReport::sum());
    if (_srp_cache_err == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_cache_err is NULL." << endl);
        return -1;
    }
    _srp_bakcenter_err = Application::getCommunicator()->getStatReport()->createPropertyReport("BackupError", PropertyReport::sum());
    if (_srp_bakcenter_err == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_bakcenter_err is NULL." << endl);
        return -1;
    }

    _srp_erasecount = Application::getCommunicator()->getStatReport()->createPropertyReport("evictedRecordCount", PropertyReport::sum());
    if (_srp_erasecount == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_erasecount is NULL." << endl);
        return -1;
    }

    _srp_erasecount_unexpire = Application::getCommunicator()->getStatReport()->createPropertyReport("evictedCountOfUnexpiredRecord", PropertyReport::sum());
    if (_srp_erasecount_unexpire == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_erasecount_unexpire is NULL." << endl);
        return -1;
    }

    _srp_getCount = Application::getCommunicator()->getStatReport()->createPropertyReport("ReadRecordCount", PropertyReport::sum());
    if (_srp_getCount == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_getCount is NULL." << endl);
        return -1;
    }

    _srp_setCount = Application::getCommunicator()->getStatReport()->createPropertyReport("WriteRecordCount", PropertyReport::sum());
    if (_srp_setCount == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_setCount is NULL." << endl);
        return -1;
    }

    _srp_coldDataRatio = Application::getCommunicator()->getStatReport()->createPropertyReport("ProportionOfColdData", PropertyReport::max());
    if (_srp_coldDataRatio == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_coldDataRatio is NULL." << endl);
        return -1;
    }

    _srp_expirecount = Application::getCommunicator()->getStatReport()->createPropertyReport("expiredRecordCount", PropertyReport::sum());
    if (_srp_expirecount == 0)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|_srp_expirecount is NULL." << endl);
        return -1;
    }

    return 0;
}

void PPReport::report(enum PropertyType type, int value)
{
    try
    {
        switch (type)
        {
        case SRP_EX:
            _srp_ex->report(value);
            break;
        case SRP_DB_EX:
            _srp_dbex->report(value);
            break;
        case SRP_DB_ERR:
            _srp_dberr->report(value);
            break;
        case SRP_CACHE_ERR:
            _srp_cache_err->report(value);
            break;
        case SRP_BINLOG_ERR:
            _srp_binlogerr->report(value);
            break;
        case SRP_BAKCENTER_ERR:
            _srp_bakcenter_err->report(value);
            break;
        case SRP_ERASE_CNT:
            _srp_erasecount->report(value);
            break;
        case SRP_ERASE_UNEXPIRE_CNT:
            _srp_erasecount_unexpire->report(value);
            break;
        case SRP_GET_CNT:
            _srp_getCount->report(value);
            break;
        case SRP_SET_CNT:
            _srp_setCount->report(value);
            break;
        case SRP_COLD_RATIO:
            _srp_coldDataRatio->report(value);
            break;
        case SRP_EXPIRE_CNT:
            _srp_expirecount->report(value);
            break;
        default:
            TLOG_ERROR("PPReport::" << __FUNCTION__ << "|unknow type:" << type << endl);
            break;
        }
    }
    catch (const std::exception &ex)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|ex:" << ex.what() << endl);
    }
    catch (...)
    {
        TLOG_ERROR("PPReport::" << __FUNCTION__ << "|unknow exception." << endl);
    }

}

GlobalStat::GlobalStat()
    :_serverType(SLAVE)
{}

GlobalStat::~GlobalStat()
{}

int GlobalStat::init()
{
    _binLogSyncTime.tSync = 0;
    _binLogSyncTime.tLast = 0;

    _hitIndex = 0;
    _slaveCreating = false;
    _enableExpire = false;
    return 0;
}

enum DCache::ServerType GlobalStat::serverType()
{
    return _serverType;
}

void GlobalStat::setServerType(enum DCache::ServerType type)
{
    _serverType = type;
}

const string& GlobalStat::groupName()
{
    return _groupName;
}
void GlobalStat::setGroupName(string &name)
{
    _groupName = name;
}

unsigned int GlobalStat::getBinlogTimeSync()
{
    return _binLogSyncTime.tSync;
}
unsigned int GlobalStat::getBinlogTimeLast()
{
    return _binLogSyncTime.tLast;
}
void GlobalStat::setBinlogTime(unsigned int sync, unsigned int last)
{
    if (sync > _binLogSyncTime.tSync)
        _binLogSyncTime.tSync = sync;
    if (last > _binLogSyncTime.tLast)
        _binLogSyncTime.tLast = last;
}

int GlobalStat::genHitIndex()
{
    return ++_hitIndex;
}

int GlobalStat::hitIndex()
{
    return _hitIndex;
}

void GlobalStat::tryHit(int index)
{
    ++_hitCount[index].totalCount;
}
void GlobalStat::hit(int index)
{
    ++_hitCount[index].hitCount;
}

void GlobalStat::getHitCnt(int index, HitCount &count)
{
    count = _hitCount[index];
}

void GlobalStat::resetHitCnt(int index)
{
    _hitCount[index].totalCount = 0;
    _hitCount[index].hitCount = 0;
    _hitCount[index].t = TNOW;
}

bool GlobalStat::isSlaveCreating()
{
    return _slaveCreating;
}
void GlobalStat::setSlaveCreating(bool stat)
{
    _slaveCreating = stat;
}

bool GlobalStat::isExpireEnabled()
{
    return _enableExpire;
}
void GlobalStat::enableExpire(bool enable)
{
    _enableExpire = enable;
}

CanSync& GlobalStat::getCanSync()
{
    return _canSync;
}


}



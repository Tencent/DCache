#include "MysqlTimeOutHandle.h"


MysqlTimeOutHandle::MysqlTimeOutHandle(CheckMysqlTimeoutInfo * timeoutInfo,const string &mysqlIp,const string &mysqlport):
 _timeoutInvoke(0)
, _totalInvoke(0)
, _frequenceFailInvoke(0)
, _lastFinishInvokeTime(TC_TimeProvider::getInstance()->getNow())
, _lastRetryTime(0)
, _activeStatus(true)
, _mysqlIp(mysqlIp)
, _mysqlPort(mysqlport)
{
	_timeoutInfo = timeoutInfo;
}

void MysqlTimeOutHandle::finishInvoke(bool bTimeout)
{
    TC_LockT<TC_ThreadMutex> lock(*this);

    time_t now = TC_TimeProvider::getInstance()->getNow();

    _frequenceFailInvoke = (bTimeout? (_frequenceFailInvoke + 1) : 0);
        
    //处于异常状态且重试成功,恢复正常状态
    if (!_activeStatus && !bTimeout)
    {
        _activeStatus = true;

        _lastFinishInvokeTime = now;

        _frequenceFailInvoke = 0;

        _totalInvoke = 1;

        _timeoutInvoke = 0;

        LOG->info() << "[dbaccess][finishInvoke, " << _mysqlIp << ":" << _mysqlPort << ", retry ok]" << endl;

        return;
    }

    //处于异常状态且重试失败
    if (!_activeStatus)
    {
        LOG->info() << "[dbaccess][finishInvoke, " << _mysqlIp << ", " << _mysqlPort << ", retry fail]" << endl;

        return;
    }

    ++_totalInvoke;

    if (bTimeout) 
    { 
        ++_timeoutInvoke; 
    }
    //判断是否进入到下一个计算时间片  
    uint32_t interval = uint32_t(now - _lastFinishInvokeTime);
    if (interval >= _timeoutInfo->checkTimeoutInterval || (_frequenceFailInvoke >= _timeoutInfo->frequenceFailInvoke && interval > 5)) //至少大于5秒
    {
        _lastFinishInvokeTime = now;

        //计算是否超时比例超过限制了
        if (_timeoutInvoke >= _timeoutInfo->minTimeoutInvoke && (_timeoutInvoke >= ((_timeoutInfo->radio * _totalInvoke)/100)))
        {
            _activeStatus = false;

            LOG->error() << "[dbaccess][finishInvoke, " 
                         << _mysqlIp << ":" << _mysqlPort 
                         << ",disable,freqtimeout:" << _frequenceFailInvoke 
                         << ",timeout:"<< _timeoutInvoke 
                         << ",total:" << _totalInvoke << "] " << endl;

            //导致死锁 屏蔽掉
            //TAF_NOTIFY_ERROR(_endpoint.desc() + ": disabled because of too many timeout.");

            resetInvoke();
        }
        else
        {
            _frequenceFailInvoke = 0;

            _totalInvoke = 0;

            _timeoutInvoke = 0;
        }
    }
}

bool MysqlTimeOutHandle::checkActive()
{
    TC_LockT<TC_ThreadMutex> lock(*this);

    time_t now = TC_TimeProvider::getInstance()->getNow();

    LOG->info() << "[dbaccess][checkActive," 
                << _mysqlIp << ":" << _mysqlPort << "," 
                << (_activeStatus ? "enable" : "disable") 
                << ",freqtimeout:" << _frequenceFailInvoke 
                << ",timeout:" << _timeoutInvoke 
                << ",total:" << _totalInvoke << "]" << endl;

    //失效且没有到下次重试时间, 直接返回不可用
    if(!_activeStatus && (now - _lastRetryTime < (int)_timeoutInfo->tryTimeInterval))
    {
        return false;
    }

    _lastRetryTime = now;

	return true; 
}

void MysqlTimeOutHandle::resetInvoke()
{
    _lastFinishInvokeTime = TC_TimeProvider::getInstance()->getNow();

    _lastRetryTime = TC_TimeProvider::getInstance()->getNow();

    _frequenceFailInvoke = 0;

    _totalInvoke = 0;

    _timeoutInvoke = 0;

  

    LOG->info() << "[dbaccess][resetInvoke, " << _mysqlIp << ":" << _mysqlPort << ", close]" << endl;
}


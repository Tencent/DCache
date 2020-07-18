#ifndef __MysqlTimeOutHandle_H_
#define __MysqlTimeOutHandle_H_

#include "util/tc_common.h"
#include "Globe.h"

using namespace tars;
struct CheckMysqlTimeoutInfo;

class MysqlTimeOutHandle : public TC_ThreadMutex 
{
public:
	 MysqlTimeOutHandle(CheckMysqlTimeoutInfo * timeoutInfo,const string &mysqlIp,const string &mysqlport);

	 bool checkActive();

	 void finishInvoke(bool bTimeout);
protected:

    void resetInvoke();
private:

    uint32_t _timeoutInvoke;

    uint32_t _totalInvoke;

    uint32_t _frequenceFailInvoke;

    time_t _lastFinishInvokeTime;

    time_t _lastRetryTime;

    bool _activeStatus;

	CheckMysqlTimeoutInfo * _timeoutInfo;

	string _mysqlIp;

	string _mysqlPort;
};

#endif


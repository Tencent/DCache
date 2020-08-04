#ifndef MK_DB_OPERATOR_H_
#define MK_DB_OPERATOR_H_

#include "CacheShare.h"
#include "tc_mysql.h"
#include "DbAccess.h"
#include "Globe.h"

using namespace tars;
using namespace std;
using namespace DCache;

class MKDbOperator
{
public:
    //typedef TC_Functor<string, TL::TLMaker<const string &>::Result> table_functor;
	typedef std::function<string(const string &)> table_functor;
    //typedef TC_Functor<TC_Mysql *, TL::TLMaker<const string &, const string &,string &,string &>::Result> db_functor;
	typedef std::function<TC_Mysql *(const string &, const string &, string &, string &)> db_functor;
	//typedef TC_Functor<int, TL::TLMaker<const string &>::Result> timeout_functor;

    void set_table_functor(table_functor tablef)
    {
        _tablef = tablef;
    }

    void set_db_functor(db_functor dbf)
    {
        _dbf = dbf;
    }
	void set_select_limit(size_t iSelectLimit)
	{
		_iSelectLimit = iSelectLimit;
	}
    void set_order(bool b)
	{
		_bOrder = b;
	}
    void set_order_item(string &item)
	{
		_orderItem = item;
	}
    void set_order_asc(bool b)
    {
        _basc = b;
    }
	tars::Int32 select(const string &mainKey, const string &field, 
		const vector<DbCondition> &vtCond, vector<map<string, string> > &vtData);

	tars::Int32 replace(const string &mainKey, const map<string, DbUpdateValue> &mpValue, const vector<DbCondition> &vtCond);

	tars::Int32 del(const string &mainKey, const vector<DbCondition> &vtCond);

protected:
	// 根据主key获取数据库连接
	TC_Mysql* getDb(string &sDbName, string &sTableName, string &mysqlNum,const string &mainKey,const string &op, string &result);
	// 操作符枚举换成操作符串
	string OP2STR(Op op);
	// DCache数据类型与TC_MySql数据类型转换
	TC_Mysql::FT DT2FT(DataType type);
	// 生成查询条件SQL语句，返回结果是 "where a=b" 这样的形式
	string buildConditionSQL(const vector<DbCondition> &vtCond, TC_Mysql *pMysql);
	// 生成更新SQL语句，返回结果是 "a=b, c=c+1" 这样的形式
	string buildUpdateSQL(const map<string, DbUpdateValue> &mpValue, TC_Mysql *pMysql);

private:
    table_functor _tablef;
    db_functor _dbf;
	size_t _iSelectLimit;
    string _orderItem;
    bool _basc;
    bool _bOrder;
	/*timeout_functor _timeoutSetf;
	timeout_functor _timeoutCleanf;*/
};

#endif

#ifndef _DbAccessImp_H_
#define _DbAccessImp_H_

#include "servant/Application.h"
#include "DbAccess.h"
#include "tc_mysql.h"
#include "util/tc_config.h"
#include "util/tc_common.h"
#include "MKDbOperator.h"
#include "Globe.h"
#include <algorithm>


/**
 *
 *
 */
class DbAccessImp : public DCache::DbAccess
{
public:
	//typedef TC_Functor<string, TL::TLMaker<const string &>::Result> table_functor;
	typedef std::function<string(const string &)> table_functor;
    //typedef TC_Functor<TC_Mysql *, TL::TLMaker<const string &, const string &, string &,string &>::Result> db_functor;
	typedef std::function<TC_Mysql *(const string &, const string &, string &, string &)> db_functor;
	//typedef TC_Functor<int, TL::TLMaker<const string &>::Result> timeout_functor;
	//typedef TC_Functor<int, TL::TLMaker<const string &>::Result> timeoutClean_functor;
	/**
	 *
	 */
	DbAccessImp(){};
	virtual ~DbAccessImp() {}

	/**
	 *
	 */
	virtual void initialize();

	/**
	 *
	 */
	virtual void destroy();

	/**
	 *
	 */
	virtual taf::Int32 getString(const std::string & keyItem, std::string &value, taf::JceCurrentPtr current);

	virtual taf::Int32 getStringExp(const std::string & keyItem, std::string &value, taf::Int32 &expireTime, taf::JceCurrentPtr current);//@2016.3.18
       

	virtual taf::Int32 getInt(taf::Int32 keyItem,std::string &value,taf::JceCurrentPtr current);

	virtual taf::Int32 getIntExp(taf::Int32 keyItem, std::string &value, taf::Int32 &expireTime, taf::JceCurrentPtr current);//@2016.3.18
        

	virtual taf::Int32 getLong(taf::Int64 keyItem, std::string &value, taf::JceCurrentPtr current);

	virtual taf::Int32 getLongExp(taf::Int64 keyItem, std::string &value, taf::Int32 &expireTime, taf::JceCurrentPtr current);//@2016.3.18

       

	virtual taf::Int32 getStringBS(const vector<taf::Char> & keyItem, vector<taf::Char> &value, taf::JceCurrentPtr current);

	virtual taf::Int32 getStringBSExp(const vector<taf::Char> & keyItem, vector<taf::Char> &value, taf::Int32 &expireTime, taf::JceCurrentPtr current); //@2016.3.18
      

	virtual taf::Int32 getIntBS(taf::Int32 keyItem, vector<taf::Char> &value, taf::JceCurrentPtr current);

	virtual taf::Int32 getIntBSExp(taf::Int32 keyItem, vector<taf::Char> &value, taf::Int32 &expireTime, taf::JceCurrentPtr current); //@2016.3.18
       

	virtual taf::Int32 getLongBS(taf::Int64 keyItem, vector<taf::Char> &value, taf::JceCurrentPtr current);

	virtual taf::Int32 getLongBSExp(taf::Int64 keyItem, vector<taf::Char> &value, taf::Int32 &expireTime, taf::JceCurrentPtr current); //@2016.3.18
       

	virtual taf::Int32 setString(const std::string & keyItem,const std::string & value,taf::JceCurrentPtr current);
       

	virtual taf::Int32 setInt(taf::Int32 keyItem,const std::string & value,taf::JceCurrentPtr current);
      

	virtual taf::Int32 setLong(taf::Int64 keyItem,const std::string & value,taf::JceCurrentPtr current);
    
	virtual taf::Int32 setStringBS(const vector<taf::Char> & keyItem,const vector<taf::Char> & value,taf::JceCurrentPtr current);
       
	virtual taf::Int32 setIntBS(taf::Int32 keyItem,const vector<taf::Char> & value,taf::JceCurrentPtr current);
       

	virtual taf::Int32 setLongBS(taf::Int64 keyItem,const vector<taf::Char> & value,taf::JceCurrentPtr current);
       
	virtual taf::Int32 delString(const std::string & keyItem,taf::JceCurrentPtr current);
      

	virtual taf::Int32 delInt(taf::Int32 keyItem,taf::JceCurrentPtr current);
      

	virtual taf::Int32 delLong(taf::Int64 keyItem,taf::JceCurrentPtr current);
       

	virtual taf::Int32 delStringBS(const vector<taf::Char> & keyItem,taf::JceCurrentPtr current);
       

	virtual taf::Int32 select(const std::string & mainKey,const std::string & field,const vector<DCache::DbCondition> & vtCond,vector<map<std::string, std::string> > &vtData,taf::JceCurrentPtr current);
      

	virtual taf::Int32 selectBS(const vector<taf::Char> & mainKey,const std::string & field,const vector<DCache::DbConditionBS> & vtCond,vector<map<std::string, vector<taf::Char> > > &vtData,taf::JceCurrentPtr current);
       

	virtual taf::Int32 replace(const std::string & mainKey,const map<std::string, DCache::DbUpdateValue> & mpValue,const vector<DCache::DbCondition> & vtCond,taf::JceCurrentPtr current);
       

	virtual taf::Int32 replaceBS(const vector<taf::Char> & mainKey,const map<std::string, DCache::DbUpdateValueBS> & mpValue,const vector<DCache::DbConditionBS> & vtCond,taf::JceCurrentPtr current);
      

	virtual taf::Int32 del(const std::string & mainKey,const vector<DCache::DbCondition> & vtCond,taf::JceCurrentPtr current);
      

	virtual taf::Int32 delBS(const vector<taf::Char> & mainKey,const vector<DCache::DbConditionBS> & vtCond,taf::JceCurrentPtr current);

	void setMysqlTimeoutRetry(TC_Mysql* mysql)
	{
		mysql->setTimeoutRetry(true);
	}

private:
	/*template<class T> void getValue（T &keyItem,std::string &value,bool isStr);*/
	template<class T> taf::Int32 getValue(const T &keyItem, std::string&value, bool isStr);
	template<class T> taf::Int32 getValue(const T &keyItem, std::string&value, taf::Int32 &expireTime, bool isStr); //@2016.3.18
	template<class T> taf::Int32 delValue(const T &keyItem,bool isStr);
	template<class T> taf::Int32 setValue(const T &keyItem,const std::string & value,bool isStr);
    MKDbOperator _mkDbopt; //二期接口的接口具体实现
	DefaultTableNameGen _tablef; 
	DefaultDbConnGen _dbf;
	bool _isMKDBAccess; //是否是二期的
	bool _isSerializated;//一期的cache存储value是否经过序列化
	string _sigKeyNameInDB;//一期的主key在DB中的字段名
	vector<FieldInfo> _vtFieldInfo;//一期的value字段格式（序列化和非序列化都用这个结构存储）
	table_functor _tableFunctor;
    db_functor _dbFunctor; 
};
/////////////////////////////////////////////////////
#endif


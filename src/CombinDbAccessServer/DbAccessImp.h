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
	*查找key对应的值是多少
	*/
	int get( const string &keyItem, string &value, int &expireTime , tars::TarsCurrentPtr current );
	
	/**
	* 设置key-value。
	*/
	int set( const string & keyItem, const string &value, int expireTime, tars::TarsCurrentPtr current  );
	
	/**
	* 删除key对应的值
	*/
	int del( const string & keyItem , tars::TarsCurrentPtr current );
	
	/***********************
	******for MKVCache *****
	**********************/
	
	/**
	* 从数据库查询记录
	* @param mainKey, 主key
	* @param field, 以","分隔的字段名，"*"表示查询所有字段
	* @param vtCond, 查询条件
	* @param vtData, 返回查询结果
	*
	* @return int, 返回记录数, 0表示没有数据，< 0表示出错
	*/
	int select( const string & mainKey, const string & field, const vector<DbCondition> &vtCond, vector<map<string, string>> &vtData , tars::TarsCurrentPtr current );
	
	/**
	* 更新数据库记录
	* @param mainKey, 主key
	* @param mpValue, 需要更新的字段
	* @param vtCond, 更新条件
	*
	* @return int, 返回更新的记录数, 0表示无影响，< 0表示出错
	*/
	int replace( const string & mainKey, const map<string, DbUpdateValue> &mpValue, const vector<DbCondition> &vtCond , tars::TarsCurrentPtr current );
	
	/**
	* 删除数据库记录
	* @param mainKey, 主key
	* @param vtCond, 删除条件
	*
	* @return int, 返回删除的记录数, 0表示无影响，< 0表示出错
	*/
	int delCond( const string & mainKey, const vector<DbCondition> &vtCond, tars::TarsCurrentPtr current );
	
	/**
	 *
	 */
	// virtual tars::Int32 getString(const std::string & keyItem, std::string &value, tars::JceCurrentPtr current);

	// virtual tars::Int32 getStringExp(const std::string & keyItem, std::string &value, tars::Int32 &expireTime, tars::JceCurrentPtr current);//@2016.3.18
       

	// virtual tars::Int32 getInt(tars::Int32 keyItem,std::string &value,tars::JceCurrentPtr current);

	// virtual tars::Int32 getIntExp(tars::Int32 keyItem, std::string &value, tars::Int32 &expireTime, tars::JceCurrentPtr current);//@2016.3.18
        

	// virtual tars::Int32 getLong(tars::Int64 keyItem, std::string &value, tars::JceCurrentPtr current);

	// virtual tars::Int32 getLongExp(tars::Int64 keyItem, std::string &value, tars::Int32 &expireTime, tars::JceCurrentPtr current);//@2016.3.18

       

	// virtual tars::Int32 getStringBS(const vector<tars::Char> & keyItem, vector<tars::Char> &value, tars::JceCurrentPtr current);

	// virtual tars::Int32 getStringBSExp(const vector<tars::Char> & keyItem, vector<tars::Char> &value, tars::Int32 &expireTime, tars::JceCurrentPtr current); //@2016.3.18
      

	// virtual tars::Int32 getIntBS(tars::Int32 keyItem, vector<tars::Char> &value, tars::JceCurrentPtr current);

	// virtual tars::Int32 getIntBSExp(tars::Int32 keyItem, vector<tars::Char> &value, tars::Int32 &expireTime, tars::JceCurrentPtr current); //@2016.3.18
       

	// virtual tars::Int32 getLongBS(tars::Int64 keyItem, vector<tars::Char> &value, tars::JceCurrentPtr current);

	// virtual tars::Int32 getLongBSExp(tars::Int64 keyItem, vector<tars::Char> &value, tars::Int32 &expireTime, tars::JceCurrentPtr current); //@2016.3.18
       

	// virtual tars::Int32 setString(const std::string & keyItem,const std::string & value,tars::JceCurrentPtr current);
       

	// virtual tars::Int32 setInt(tars::Int32 keyItem,const std::string & value,tars::JceCurrentPtr current);
      

	// virtual tars::Int32 setLong(tars::Int64 keyItem,const std::string & value,tars::JceCurrentPtr current);
    
	// virtual tars::Int32 setStringBS(const vector<tars::Char> & keyItem,const vector<tars::Char> & value,tars::JceCurrentPtr current);
       
	// virtual tars::Int32 setIntBS(tars::Int32 keyItem,const vector<tars::Char> & value,tars::JceCurrentPtr current);
       

	// virtual tars::Int32 setLongBS(tars::Int64 keyItem,const vector<tars::Char> & value,tars::JceCurrentPtr current);
       
	// virtual tars::Int32 delString(const std::string & keyItem,tars::JceCurrentPtr current);
      

	// virtual tars::Int32 delInt(tars::Int32 keyItem,tars::JceCurrentPtr current);
      

	// virtual tars::Int32 delLong(tars::Int64 keyItem,tars::JceCurrentPtr current);
       

	// virtual tars::Int32 delStringBS(const vector<tars::Char> & keyItem,tars::JceCurrentPtr current);
       

	// virtual tars::Int32 select(const std::string & mainKey,const std::string & field,const vector<DCache::DbCondition> & vtCond,vector<map<std::string, std::string> > &vtData,tars::JceCurrentPtr current);
      

	// virtual tars::Int32 selectBS(const vector<tars::Char> & mainKey,const std::string & field,const vector<DCache::DbConditionBS> & vtCond,vector<map<std::string, vector<tars::Char> > > &vtData,tars::JceCurrentPtr current);
       

	// virtual tars::Int32 replace(const std::string & mainKey,const map<std::string, DCache::DbUpdateValue> & mpValue,const vector<DCache::DbCondition> & vtCond,tars::JceCurrentPtr current);
       

	// virtual tars::Int32 replaceBS(const vector<tars::Char> & mainKey,const map<std::string, DCache::DbUpdateValueBS> & mpValue,const vector<DCache::DbConditionBS> & vtCond,tars::JceCurrentPtr current);
      

	// virtual tars::Int32 del(const std::string & mainKey,const vector<DCache::DbCondition> & vtCond,tars::JceCurrentPtr current);
      

	// virtual tars::Int32 delBS(const vector<tars::Char> & mainKey,const vector<DCache::DbConditionBS> & vtCond,tars::JceCurrentPtr current);

	void setMysqlTimeoutRetry(TC_Mysql* mysql)
	{
		mysql->setTimeoutRetry(true);
	}

private:
	/*template<class T> void getValue（T &keyItem,std::string &value,bool isStr);*/
	template<class T> tars::Int32 getValue(const T &keyItem, std::string&value, bool isStr);
	template<class T> tars::Int32 getValue(const T &keyItem, std::string&value, tars::Int32 &expireTime, bool isStr); //@2016.3.18
	template<class T> tars::Int32 delValue(const T &keyItem,bool isStr);
	template<class T> tars::Int32 setValue(const T &keyItem,const std::string & value,bool isStr);
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


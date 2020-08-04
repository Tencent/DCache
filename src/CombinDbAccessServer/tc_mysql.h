#ifndef __TC_MYSQL_H
#define __TC_MYSQL_H

#include "mysql.h"
#include "util/tc_ex.h"
#include <map>
#include <vector>
#include <stdlib.h>
#include "util/tc_common.h"
using namespace tars;
namespace tars
{
/////////////////////////////////////////////////
// 说明: mysql操作类
// Author : jarodruan@tencent.com              
/////////////////////////////////////////////////
/**
* 数据库异常类
*/
struct TC_Mysql_Exception : public TC_Exception
{
	TC_Mysql_Exception(const string &sBuffer) : TC_Exception(sBuffer){};
	~TC_Mysql_Exception() throw(){};    
};
struct TC_Mysql_TimeOut_Exception : public TC_Exception
{
    TC_Mysql_TimeOut_Exception(const string &sBuffer) : TC_Exception(sBuffer){};
	~TC_Mysql_TimeOut_Exception() throw(){};
};

/**
* 数据库配置接口
*/
struct TC_DBConf
{
    /**
    * 主机地址
    */
    string _host;

    /**
    * 用户名
    */
    string _user;

    /**
    * 密码
    */
    string _password;

    /**
    * 数据库
    */
    string _database;

    /**
     * 字符集
     */
    string _charset;

    /**
    * 端口
    */
    int _port;

    /**
    * 客户端标识
    */
    int _flag;

	int _conTimeOut;

	int _queryTimeOut;
    /**
    * 构造函数
    */
    TC_DBConf()
        : _port(0)
        , _flag(0)
		,_conTimeOut(2)
		,_queryTimeOut(2)
    {
    }
    /**
    * 读取数据库配置
    * 主机地址
    * dbhost=
    * dbuser=
    * dbpass=
    * dbname=
    * dbport=
    * @param mpParam
    */
    void loadFromMap(const map<string, string> &mpParam)
    {
        map<string, string> mpTmp = mpParam;

        _host        = mpTmp["dbhost"];
        _user        = mpTmp["dbuser"];
        _password    = mpTmp["dbpass"];
        _database    = mpTmp["dbname"];
        _charset     = mpTmp["charset"];
        _port        = atoi(mpTmp["dbport"].c_str());
		_conTimeOut     = atoi(mpTmp["conTimeOut"].c_str());
		_queryTimeOut     = atoi(mpTmp["queryTimeOut"].c_str());
        _flag        = 0;

        if(mpTmp["dbport"] == "")
        {
            _port = 3306;
        }
    }
};

/**
* Mysql数据库操作类
*/
class TC_Mysql 
{
public:
    /**
    * contructor
    */
    TC_Mysql();

    /**
    * contructor
    * @param sHost : 主机IP
    * @param sUser : 用户
    * @param sPasswd : 密码
    * @param sDatebase : 数据库
    * @param port : 端口
    * @param iUnixSocket : socket
    * @param iFlag : 客户端标识
    */
    TC_Mysql(const string& sHost, const string& sUser = "", const string& sPasswd = "", const string& sDatabase = "", const string &sCharSet = "", int port = 0, int iFlag = 0,int timeout = 1);

    /**
    * contructor
    * @param tcDBConf : 数据库配置
    */
    TC_Mysql(const TC_DBConf& tcDBConf);

    /**
    * decontructor
    */
    ~TC_Mysql();

    /**
    * init 初始化
    * @param sHost : 主机IP
    * @param sUser : 用户
    * @param sPasswd : 密码
    * @param sDatebase : 数据库
    * @param port : 端口
    * @param iUnixSocket : socket
    * @param iFlag : 客户端标识
    * @return 无
    */
    void init(const string& sHost, const string& sUser  = "", const string& sPasswd  = "", const string& sDatabase = "", const string &sCharSet = "", int port = 0, int iFlag = 0 ,int _conTimeOut = 2,int _queryTimeOut = 2);

    /**
    * contructor
    * @param tcDBConf : 数据库配置
    */
    void init(const TC_DBConf& tcDBConf);

    /**
    * connect 连接数据库
    * @throws TC_Mysql_Exception
    * @return 无
    */
    void connect();
 
    /**
    * DisConnect 断开数据库连接
    * @return 无
    */
    void disconnect();

    /**
     * 获取数据库变量
     * 
     * @return string
     */
    string getVariables(const string &sName);

    /**
    * DisConnect 直接获取数据库指针
    * @return MYSQL* 获取数据库指针
    */
    MYSQL *getMysql();

    /**
    * 字符转义
    * @param sFrom : 源字符串
    * @param sTo : 输出字符串
    * @return string 输出字符串
    */
    string escapeString(const string& sFrom);

    /**
    * Update or insert ...
    * @param sSql : sql语句
    * @throws TC_Mysql_Exception
    * @return void
    */
    void execute(const string& sSql);

    /**
     * mysql的一条记录
     */
    class MysqlRecord
    {
    public:
        /**
         * 构造函数
         * @param record
         */
        MysqlRecord(const map<string, string> &record);

        /**
         * 获取数据
         * @param s
         * 
         * @return string
         */
        const string& operator[](const string &s);
    protected:
        const map<string, string> &_record;
    };

    /**
     * 查询出来的mysql数据
     */
    class MysqlData
    {
    public:
        /**
         * 所有数据
         * 
         * @return vector<map<string,string>>&
         */
        vector<map<string, string> >& data();

        /**
         * 数据的记录条数
         * 
         * @return size_t
         */
        size_t size();

        /**
         * 获取莫一条记录
         * @param i
         * 
         * @return MysqlRecord
         */
        MysqlRecord operator[](size_t i);

    protected:
        vector<map<string, string> > _data;
    };

    /**
    * Query Record
    * @param sSql : sql语句
    * @throws TC_Mysql_Exception
    * @return MysqlData
    */
    MysqlData queryRecord(const string& sSql);

    /**
     * 定义字段类型
     */
    enum FT
    {
        DB_INT,     //数字类型
        DB_STR,     //字符串类型
    };

    //数据记录
    typedef map<string, pair<FT, string> > RECORD_DATA;

    /**
    * Update Record
    * @param sTableName : 表名
    * @param mpColumns : 列名/值对
    * @param sCondition : where子语句,例如:where A = B
    * @throws TC_Mysql_Exception
    * @return size_t 影响的行数
    */
    size_t updateRecord(const string &sTableName, const map<string, pair<FT, string> > &mpColumns, const string &sCondition);

    /**
    * insert Record
    * @param sTableName : 表名
    * @param mpColumns : 列名/值对
    * @throws TC_Mysql_Exception
    * @return size_t 影响的行数
    */
    size_t insertRecord(const string &sTableName, const map<string, pair<FT, string> > &mpColumns);

    /**
    * replace Record
    * @param sTableName : 表名
    * @param mpColumns : 列名/值对
    * @throws TC_Mysql_Exception
    * @return size_t 影响的行数
    */
    size_t replaceRecord(const string &sTableName, const map<string, pair<FT, string> > &mpColumns);

    /**
    * Delete Record
    * @param sTableName : 表名
    * @param sCondition : where子语句,例如:where A = B
    * @throws TC_Mysql_Exception
    * @return size_t 影响的行数
    */
    size_t deleteRecord(const string &sTableName, const string &sCondition = "");

    /**
    * 获取Table查询结果的数目
    * @param sTableName : 用于查询的表名
    * @param sCondition : where子语句,例如:where A = B
    * @throws TC_Mysql_Exception
    * @return size_t 查询的记录数目
	*/
    size_t getRecordCount(const string& sTableName, const string &sCondition = "");

    /**
    * 获取Sql返回结果集的个数
    * @param sCondition : where子语句,例如:where A = B
    * @throws TC_Mysql_Exception
    * @return 查询的记录数目
	*/
    size_t getSqlCount(const string &sCondition = "");

    /**
     * 存在记录
     * @param sql
     * @throws TC_Mysql_Exception
     * @return bool
     */
    bool existRecord(const string& sql);

    /**
    * 获取字段最大值
    * @param sTableName : 用于查询的表名
    * @param sFieldName : 用于查询的字段
    * @param sCondition : where子语句,例如:where A = B
    * @throws TC_Mysql_Exception
    * @return 查询的记录数目
	*/
    int getMaxValue(const string& sTableName, const string& sFieldName, const string &sCondition = "");

    /**
    * 获取auto_increment最后插入得ID
    * @return ID值
	*/
    long lastInsertID();

    /**
    * 构造Insert-SQL语句
    * @param sTableName : 表名
    * @param mpColumns : 列名/值对
    * @return string insert-SQL语句
    */
    string buildInsertSQL(const string &sTableName, const map<string, pair<FT, string> > &mpColumns);

    /**
    * 构造Replace-SQL语句
    * @param sTableName : 表名
    * @param mpColumns : 列名/值对
    * @return string insert-SQL语句
    */
    string buildReplaceSQL(const string &sTableName, const map<string, pair<FT, string> > &mpColumns);

    /**
    * 构造Update-SQL语句
    * @param sTableName : 表名
    * @param mpColumns : 列名/值对
    * @param sCondition : where子语句
    * @return string Update-SQL语句
    */
    string buildUpdateSQL(const string &sTableName,const map<string, pair<FT, string> > &mpColumns, const string &sCondition);

    /**
     * 获取最后执行的SQL语句
     * 
     * @return string
     */
    string getLastSQL() { return _sLastSql; }

    /**
     * 设置超时是否重试
     * 
     * @return string
     */
    void setTimeoutRetry(bool bRetry) { _bTimeoutRetry = bRetry; }

protected:
    /**
    * copy contructor
    * 只申明,不定义,保证不被使用
    */
    TC_Mysql(const TC_Mysql &tcMysql);

    /**
    * =
    * 只申明,不定义,保证不被使用
    */
    TC_Mysql &operator=(const TC_Mysql &tcMysql);


private:

    /**
    * 数据库指针
    */
    MYSQL 		*_pstMql;

    /**
    * 数据库配置
    */
    TC_DBConf   _dbConf;
    
    /**
    * 是否已经连接
    */
    bool		_bConnected;

    /**
     * 最后执行的sql
     */
    string      _sLastSql;

    /**
     * 超时是否重试
     */
    bool      _bTimeoutRetry;
};

}
#endif //_TC_MYSQL_H

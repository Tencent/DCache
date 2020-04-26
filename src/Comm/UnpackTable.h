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
#ifndef _UNPACK_TABLE_H_
#define _UNPACK_TABLE_H_

#include "servant/Application.h"
#include "RouterShare.h"
#include "util/tc_file.h"
#include "util/tc_thread.h"
#include "NormalHash.h"

using namespace std;
using namespace tars;

extern int g_iSlaveFlag;//备机可读的轮询标志

// 记录异常日志
#define UNPACK_EXPLOG(e) \
	TLOGERROR(__FILE__ << "|" << __FUNCTION__ \
		<< "|" << __LINE__ << "|" << e << endl);

// 捕捉异常
#define __UNPACK_TRY__ try\
	{

// 处理异常
#define __UNPACK_CATCH__ }\
	catch (TC_Exception const& e)\
	{\
		UNPACK_EXPLOG(string("catch tc exception: ") + e.what());\
	}\
	catch (std::exception const& e)\
	{\
		UNPACK_EXPLOG(string("catch std exception: ") + e.what());\
	}\
	catch (...)\
	{\
		UNPACK_EXPLOG("catch unknown exception");\
	}

namespace DCache
{

    //解压缩的table，用于查询
    class UnpackTable
    {
    public:
        static const uint32_t __max = 0xffffffff;	// 整形最大值

        static const uint32_t __pageSize = 10000;	// 默认页大小

        //默认最大页号，如果余数不为0，增加一页
        static const uint32_t __allPageCount = __max / __pageSize + (__max % __pageSize > 0 ? 1 : 0);

        static const int64_t __reloadTime = 10000;	// 默认路由加载间隔时间(us)

        enum
        {
            RET_SUCC = 0,
            RET_NOT_FOUND_SERVER = -1,	// 找不着server
            RET_NOT_FOUND_GROUP = -2,	// 找不着group
            RET_NOT_FOUND_MASTER = -3,	// 找不着master server
            RET_NOT_FOUND_SLAVE = -4,	// 找不着slave server
            RET_NOT_FOUND_TRANS = -5,	// 找不着迁移目的server
            RET_NOT_FOUND_SRC_GROUP = -6,	// 找不着源迁移组
            RET_NOT_FOUND_SRC_MASTER = -7, 	// 找不着源迁移master
            RET_NOT_FOUND_SRC_SLAVE = -8, 	// 找不着源迁移slave
            RET_NOT_FOUND_DEST_GROUP = -9, 	// 找不着目的迁移组
            RET_NOT_FOUND_DEST_MASTER = -10, 	// 找不着目的迁移master
            RET_NOT_FOUND_DEST_SLAVE = -11, 	// 找不着目的迁移slave
            RET_GROUP_READONLY = -12, //组只读状态
            RET_EXCEPTION = -99,	// 异常
            RET_NOT_TRANSFERING = 1	// 没有正在迁移，非异常
        };
    public:
#pragma pack(1)

        /*
        * 页路由项，每个页对应一个Entry
        * 每个Entry包含4个字节，各个bit位分别表示如下:
        * 	第32位表示当前页是否属于本机服务范围
        *	第31位表示当前页是否正在迁移
        *    第16-30位表示该页所在服务器ID
        *	第1-15位表示该页正在迁移的目的服务器ID
        */
        struct Entry
        {
            Entry();

            /*设置当前页属于本机服务范围*/
            inline void setSelf(bool bSelf);

            /*判断当前页是否属于本机服务范围*/
            inline bool getSelf() const;

            /*设置当前页正在迁移标志*/
            inline void setTransfering(bool bTransfer);

            /*判断当前页是否正在迁移*/
            inline bool getTransfering() const;

            /*设置当前页所在服务器ID*/
            inline void setIndex(uint32_t idx);

            /*获取当前页所在服务器ID*/
            inline uint32_t getIndex() const;

            /*设置当前页正在迁移的目的服务器ID*/
            inline void setTransIndex(uint32_t idx);

            /*获取当前页正在迁移的目的服务器ID*/
            inline uint32_t getTransIndex() const;

            uint32_t value;	// 用于该页路由信息
        };
#pragma pack()

        struct LocalRouterInfo
        {
            LocalRouterInfo(size_t Num)
            {
                entry = new Entry[Num];
                memset(entry, 0, sizeof(Entry) * Num);
            }
            ~LocalRouterInfo() { delete[] entry; }

            PackTable packTable;	// 打包的路由信息
            Entry* entry;	// 解包后的路由信息
            map<int, GroupInfo> groupMap;	// 服务器组名，服务器组信息
            map<string, int> serverNameMap;	// 服务器名，服务器组编号

            map<int, int> routerGroupMap; //服务组编号，0；用于保存有路由的组信息，
        };

    public:
        UnpackTable();
        UnpackTable(const UnpackTable& r);	// 有用???
        ~UnpackTable();


    public:

        /**
         * 始初化
         * @param packTable 路由信息
         * @param sServerName 本地服务器名
         * @param pageSize 页的大小
         * @param reloadTime 重载及初始化的最小间隔时间(us)
         * @return int RET_SUCC, RET_NOT_FOUND_SERVER, RET_NOT_FOUND_GROUP
         */

        int init(const PackTable& packTable, const string& sServerName,
            uint32_t pageSize = __pageSize, int64_t reloadTime = __reloadTime, const vector<TransferInfo>& transferingInfoList = vector<TransferInfo>());

        /**
         * 重新加载路由信息
         * @param packTable 路由信息
         * @return int RET_SUCC, RET_NOT_FOUND_SERVER, RET_NOT_FOUND_GROUP
         */

        int reload(const PackTable& packTable, const vector<TransferInfo>& transferingInfoList = vector<TransferInfo>());

        /**
         * 获取key对应的服务器ServerInfo
         * @param key key值
         * @return ServerInfo, RET_SUCC, RET_NOT_FOUND_SERVER, RET_NOT_FOUND_GROUP
         */
        template<typename T> int getMaster(T key, ServerInfo &serverInfo) const
        {
            return getMasterByHash(hashKey(key), serverInfo);
        };

        template<typename T> int getIdcServer(T key, const string &sIdc, bool bSlaveReadAble, ServerInfo &serverInfo) const
        {
            __UNPACK_TRY__

            int iRet = RET_SUCC;

            uint32_t hash = hashKey(key);

            // 取得页号
            uint32_t pageNo = hash / _pageSize;
            assert(pageNo <= _allPageCount);

            // 根据页号获取所属服务器组编号
            int groupId = _lrInfo->entry[pageNo].getIndex();
            map<int, GroupInfo>::const_iterator it = _lrInfo->groupMap.find(groupId);
            
            if (it != _lrInfo->groupMap.end())
            {
                map<string, vector<string> >::const_iterator it_idc = it->second.idcList.find(sIdc);
                if (it_idc != it->second.idcList.end())
                {
                    //只读状态，把对主机的读都切到备机
                    if (it->second.accessStatus == 1)
                    {
                        if (it_idc->second.size() > 1)
                        {
                            serverInfo = _lrInfo->packTable.serverList[it_idc->second[1]];
                        }
                        else
                            serverInfo = _lrInfo->packTable.serverList[it_idc->second[0]];
                    }
                    //镜像不可用状态，把对镜像的读切换到备机
                    else if (it->second.accessStatus == 2)
                    {
                        serverInfo = _lrInfo->packTable.serverList[it->second.masterServer];
                        if (sIdc != serverInfo.idc)
                        {
                            //查询镜像，先要检查镜像服务状态。如果没有镜像备机并且状态不为0，则切到备机
                            ServerInfo mirrServerInfo = _lrInfo->packTable.serverList[it_idc->second[0]];
                            if ((it_idc->second.size() == 1) && (mirrServerInfo.status != 0))
                            {
                                map<string, vector<string> >::const_iterator it_idc_master = it->second.idcList.find(serverInfo.idc);
                                if (it_idc_master->second.size() > 1)
                                    serverInfo = _lrInfo->packTable.serverList[it_idc_master->second[1]];
                                else
                                    serverInfo = _lrInfo->packTable.serverList[it_idc_master->second[0]];
                            }
                            else
                            {
                                //是否选择备机的标志
                                if (bSlaveReadAble && (it_idc->second.size() >= 2) && bool(__sync_fetch_and_xor(&g_iSlaveFlag, 1)))
                                {
                                    serverInfo = _lrInfo->packTable.serverList[it_idc->second[1]];
                                    if (serverInfo.status != 0)
                                    {
                                        serverInfo = mirrServerInfo;
                                    }
                                }
                                else
                                    serverInfo = mirrServerInfo;
                            }
                        }
                        else
                        {
                            if (bSlaveReadAble && (it_idc->second.size() >= 2) && bool(__sync_fetch_and_xor(&g_iSlaveFlag, 1)))
                            {
                                serverInfo = _lrInfo->packTable.serverList[it_idc->second[1]];
                                if (serverInfo.status != 0)
                                {
                                    serverInfo = _lrInfo->packTable.serverList[it_idc->second[0]];
                                }
                            }
                            else
                                serverInfo = _lrInfo->packTable.serverList[it_idc->second[0]];
                        }
                    }
                    else
                    {
                        //是否选择备机的标志
                        if (bSlaveReadAble && (it_idc->second.size() >= 2) && bool(__sync_fetch_and_xor(&g_iSlaveFlag, 1)))
                        {
                            serverInfo = _lrInfo->packTable.serverList[it_idc->second[1]];
                            if (serverInfo.status != 0)
                            {
                                serverInfo = _lrInfo->packTable.serverList[it_idc->second[0]];
                            }
                        }
                        else
                            serverInfo = _lrInfo->packTable.serverList[it_idc->second[0]];
                    }
                }
                else
                {
                    // 该组没有位于sIdc的机器，则直接使用该组主机所在IDC的机器
                    serverInfo = _lrInfo->packTable.serverList[it->second.masterServer];
                    map<string, vector<string> >::const_iterator it_idc_master = it->second.idcList.find(serverInfo.idc);
                                 
                    if (bSlaveReadAble && (it_idc_master->second.size() >= 2) && bool(__sync_fetch_and_xor(&g_iSlaveFlag, 1)))
                    {
                        serverInfo = _lrInfo->packTable.serverList[it_idc_master->second[1]];
                        if (serverInfo.status != 0)
                        {
                            serverInfo = _lrInfo->packTable.serverList[it_idc_master->second[0]];
                        }
                    }
                    else
                        serverInfo = _lrInfo->packTable.serverList[it_idc_master->second[0]];
                }
            }
            else
            {
                iRet = RET_NOT_FOUND_GROUP;
            }

            return iRet;

            __UNPACK_CATCH__

            return RET_EXCEPTION;
        };

        /**
         * 获取hash对应的服务器ServerInfo
         * @param hash hash值
         * @return ServerInfo, RET_SUCC, RET_NOT_FOUND_SERVER, RET_NOT_FOUND_GROUP
         */
        int getMasterByHash(uint32_t hash, ServerInfo &serverInfo) const;



        /**
         * 获取所有的master server
         * @return const vector<ServerInfo>&
         */
        int getAllMasters(vector<ServerInfo> &masterServers) const;

        /**
         * 根据idc信息，获取所有的server
         * @return const vector<ServerInfo>&
         */
        int getAllIdcServer(const string &sIdc, vector<ServerInfo> &servers) const;

        /**
         * 获取组信息
         * @param sServerName 服务器名
         * @return pair<ServerInfo,ServerInfo>, RET_SUCC, RET_NOT_FOUND_SERVER,
         *	RET_NOT_FOUND_GROUP
         */
        int getGroup(const string& sServerName, GroupInfo& group) const;

        /**
         * 获取迁移源server和目的server
         * @param PageNo 路由页号
         * @param srcServer 传出参数，仅在返回值为RET_SUCC有效，表示页迁移源server
         * @param destServer 传出参数，仅在返回值为RET_SUCC有效，表示页迁移目的server
         * @return:
                   RET_SUCC 该页正在迁移
                   RET_NOT_FOUND_TRANS, 
                   RET_NOT_TRANSFERING 该页未在迁移
         */
        int getTrans(int PageNo, ServerInfo &srcServer, ServerInfo &destServer);

        /**
         * 判断是否是迁移源地址
         * @param PageNo
         * @param sServerName
         * @return true or false
        */
        bool isTransSrc(int PageNo, const string& sServerName);

        /**
        * 获取备份源机server
        * @param sServerName 服务器名
        * @return bakSourceServer, RET_SUCC, RET_NOT_FOUND_SERVER
        */
        int getBakSource(const string& sServerName, ServerInfo &bakSourceServer);

        /**
         * 获取当前路由版本
         * @param
         * @return int
         */
        int getVersion();

        /*
        * 获取PackTable
        */
        const PackTable& getPackTable() const
        {
            return _lrInfo->packTable;
        }

        /*
        * 获取selfServer
        */
        const string& getSelfServer() const
        {
            return _selfServer;
        }

        /*
        * 获取页大小
        */
        const uint32_t getPageSize() const
        {
            return _pageSize;
        }

        /*
        * 获取重载间隔时间
        */
        const int64_t getReloadTime() const
        {
            return _reloadTime;
        }

    public:

        /**
         * key是否自己维护的号段
         * @param key key值
         * @return bool
         */
        template<typename T> bool isMySelf(T key) const
        {
            return isMySelfByHash(hashKey(key));
        };


        /**
         * hash是否自己维护的号段
         * @param hash hash值
         * @return bool
         */
        bool isMySelfByHash(uint32_t hash) const;

        /**
         * 判断是否正在迁移中
         * @param key
         * @return bool
         */
        template<typename T> bool isTransfering(T key) const
        {
            return isTransferingByHash(hashKey(key));
        };
        bool isTransferingByHash(uint32_t hash) const;


        /**
         *  打印路由表
         * @param os
         */
        void print(ostream& os) const;

        /*
        * 清除路由信息
        */
        void clear();


        /**
         * 使用文件初始化路由表
         * @param fileName
         * @param selfServer
         * @return int
         */
        int fromFile(const string& fileName, const string& sServerName);

        /**
         * 路由表保存成文件
         * @param fileName
         * @return int
         */
        int toFile(const string& fileName) const;

        /**
        * 对路由信息字符串化
         * @return string
         */
        string toString() const;

        /**
         * 对uint32_t key进行hash
         * @param key
         * @return size_t
         */
        size_t hashKey(uint32_t key) const;

        /**
         * 对uint64_t key进行hash
         * @param key
         * @return size_t
         */
        size_t hashKey(uint64_t key) const;

        /**
         * 对string key进行hash
         * @param key
         * @return size_t
         */
        size_t hashKey(string key) const;

        /**
         * 获取页号
         * @param key
         * @return size_t
         */
        template<typename T> size_t getPageNo(T key) const
        {
            return (hashKey(key) / _pageSize);
        };

    private:
        LocalRouterInfo* _lrInfo;	// 本地路由信息
        LocalRouterInfo* _lrInfo1;	// 备份，用于路由重载切换

        string _selfServer;	// 保存本机名称
        bool _init;	// 是否已初始化标志
        int64_t _iLastLoadTime;	// 最后路由加载时间
        P_Hash* _hash;	// hash处理类

        TC_ThreadLock _lock;	// 加载路由时加锁

        uint32_t _pageSize;	// 页大小，可以初始化时传入
        uint32_t _allPageCount;	// 最大页号，根据页大小计算
        int64_t _reloadTime;	// 重载路由的最小间隔时间(us)
    };

    void UnpackTable::Entry::setSelf(bool bSelf)
    {
        bSelf ? value |= (0x01 << (sizeof(value) * 8 - 1)) : value &= ~(0x01 << (sizeof(value) * 8 - 1));
    }

    bool UnpackTable::Entry::getSelf() const
    {
        return value & (0x01 << (sizeof(value) * 8 - 1));
    }

    void UnpackTable::Entry::setTransfering(bool flag)
    {
        flag ? value |= (0x01 << (sizeof(value) * 8 - 2)) : value &= ~(0x01 << (sizeof(value) * 8 - 2));
    }

    bool UnpackTable::Entry::getTransfering() const
    {
        return value & (0x01 << (sizeof(value) * 8 - 2));
    }

    void UnpackTable::Entry::setIndex(uint32_t idx)
    {
        assert(idx < (0x01 << (sizeof(value) * 8 - 2) / 2));

        value |= (idx << (sizeof(value) * 8 - 2) / 2);
    }

    uint32_t UnpackTable::Entry::getIndex() const
    {
        //取第16-30位
        return (uint32_t)((value & (((0x01 << (sizeof(value) * 8 - 2) / 2) - 1) << (sizeof(value) * 8 - 2) / 2)) >> (sizeof(value) * 8 - 2) / 2);
    }

    void UnpackTable::Entry::setTransIndex(uint32_t idx)
    {
        assert(idx < (0x01 << (sizeof(value) * 8 - 2) / 2));

        value |= idx;
    }

    uint32_t UnpackTable::Entry::getTransIndex() const
    {
        // 取第1-15位
        return (uint32_t)(value & ((0x01 << (sizeof(value) * 8 - 2) / 2) - 1));
    }

    //////////////////////////////////////////////////////////////
}
#endif

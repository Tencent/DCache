This document describes and examples the configuration of each module of DCache.

# ConfigServer Configuration

```
# database infomation
<Main>
    <DB>
        dbhost=
        dbpass=
        dbuser=
        dbname=db_dcache_relation
        charset=
        dbport=
    </DB>
</Main>
```

# KVCacheServer Configuration
```
<Main>
    # module name
    ModuleName=Test

    # coredump size
    CoreSizeLimit=-1
    BackupDayLog=dumpAndRecover
    # interval (in milliseconds) for reporting heartbeats to the Router
    RouterHeartbeatInterval=1000
    <Cache>
        # key for shared memory
        ShmKey=12345
        # shared memory size
        ShmSize=10M
        # average data size, used to initialize the shared memory space
        AvgDataSize=1
        # Hash ratio, (chunk block) = HashRadio * (hash item)
        HashRadio=2

        EnableErase=Y
        # interval for erasing data(second)
        EraseInterval=5
        # the rate at which data is erased (used chunks/all chunks)
        EraseRadio=95
        EraseThreadCount=2
        # the max number of records to be erase each time
        MaxEraseCountOneTime=500

        # whether to start a thread for clearing expired data
        StartExpireThread=Y
        # whether to erase data stored in on-disk database
        ExpireDb=Y
        # interval for clearing expired data(second)
        ExpireInterval=300
        # frequency of data removal, 0 means no limit
        ExpireSpeed=0

        # writeback interval (second)
        SyncInterval=300
        # writeback frequency, 0 means no limit
        SyncSpeed=0
        # number of threads for writting back
        SyncThreadNum=1
        # writeback time (second)
        SyncTime=300
        # time periods that forbidding writting back (Example: 0900-1000;1600-1700)
        SyncBlockTime=0000-0000

        # whether to save OnlyKey data Y/N
        SaveOnlyKey=Y
        # host auto-downgrade timeout (second), that is, automatically downgrade without heartbeat within 30 seconds
        DowngradeTimeout=30

        # number of memory blocks
        JmemNum=10
        # whether to statistic the proportion of cold data
        coldDataCalEnable=Y
        # cold data statistics period (day)
        coldDataCalCycle=7
        # the maximum length of Key in back-end database
        MaxKeyLengthInDB=767
    </Cache>
    <Log>
        DbDayLog=db
    </Log>
    <DbAccess>
        # is there a on-disk database? Y/N
        DBFlag=Y
        ObjName=DCache.TestDbAccessServer.DbAccessObj
        # whether to query from DB or file when there is no data in the Cache, Y/N
        ReadDbFlag=Y
    </DbAccess>
    <BinLog>
        # the suffix of name of binlog file
        LogFile=binlog
        # whether to record binlog
        Record=Y
        # whether to record key binlog
        KeyRecord=N
        # the number of rows of the binlog that need to be synchronized each time
        MaxLine=10000
        # whether to compress binlog when synchronizing
        SyncCompress=Y
        # whether to use gzip
        IsGzip=Y
        # whether to use key binlog when master-slave synchronization
        KeySyncMode=N

        # the size of the cache used by SLAVE to receive the binlog
        BuffSize=10
        # interval for saving synctime file(second)
        SaveSyncTimeInterval=10
        # the period of active binlog generation (second)
        HeartbeatInterval=600
    </BinLog>
    <Router>
        ObjName=DCache.TestRouterServer.RouterObj
        PageSize=10000
        # the name of local file for saving routing table
        RouteFile=Route.dat
        # interval for synchronizing routing table (second)
        SyncInterval=1
    </Router>
</Main>
```
# MKVCacheServer Configuration
```
<Main>
    ModuleName=Test

    # coredump size
    CoreSizeLimit=-1
    # the max number of record returned when query by mainKey
    MKeyMaxBlockCount=20000
    BackupDayLog=dumpAndRecover
    # interval (in milliseconds) for reporting heartbeats to the Router
    RouterHeartbeatInterval=1000
    <Cache>
        # key for shared memory
        ShmKey=692815670
        # shared memory size
        ShmSize=10M
        # average data size, used to initialize the shared memory space
        AvgDataSize=1
        # Hash ratio, (chunk block) = HashRadio * (hash item)
        HashRadio=2
        # ratio of the number of elements to the number of mainKeys
        MKHashRadio=1
        # datatypes，hash/set/zset/list
        MainKeyType=hash

        # whether to start thread for deleting data
        StartDeleteThread=Y
        # interval for deleting data(second)
        DeleteInterval=300
        # delete speed, 0 means unlimited speed
        DeleteSpeed=0

        EnableErase=Y
        # interval for erasing data(second)
        EraseInterval=5
        # the rate at which data is erased (used chunks/all chunks)
        EraseRadio=95
        # number of threads used to erase data 
        EraseThreadCount=2
        # the max number of records to be erase each time
        MaxEraseCountOneTime=500

        # whether to start a thread for clearing expired data
        StartExpireThread=Y
        # whether to erase data stored in on-disk database
        ExpireDb=N
        # interval for clearing expired data(second)
        ExpireInterval=3600
        # frequency of data removal, 0 means no limit
        ExpireSpeed=0

        # writeback interval (second)
        SyncInterval=300
        # writeback frequency, 0 means no limit
        SyncSpeed=0
        # number of threads for writting back
        SyncThreadNum=1
        # writeback time (second)
        SyncTime=300
        # time periods that forbidding writting back (example：0900-1000;1600-1700)
        SyncBlockTime=0000-0000
        SyncUNBlockPercent=60

        # Y/N <==> head/tail, 'Y' means add element into head of mainKey chain, 'N' means add element into tail of mainKey chain 
        InsertOrder=N
        # whether to update the order of mainKey chain when update data
        UpdateOrder=N
        # the maximum number of records with the same mainKey, if exceeded, the old data will be deleted. 0 means no limit
        MkeyMaxDataCount=0
        # whether to save onlyKey data
        SaveOnlyKey=Y
        # insert data queried from db  into mainKey chain in cache  according to the order of the fields specified by OrderItem
        OrderItem=
        # whether to sort in descending order
        OrderDesc=Y

        # number of memory blocks
        JmemNum=2
        # host auto-downgrade timeout (second), that is, automatically downgrade without heartbeat within 30 seconds
        DowngradeTimeout=30
        # whether to statistic the proportion of cold data
        coldDataCalEnable=Y
        # cold data statistics period (day)
        coldDataCalCycle=7
        # the maximum length of Key in back-end database
        MaxKeyLengthInDB=767
        # whethe to compress data when migrating
        transferCompress=Y
    </Cache>
    <Log>
        DbDayLog=db
    </Log>
    <DbAccess>
        # is there a on-disk database? Y/N
        DBFlag=Y
        ObjName=DCache.TestDbAccessServer.DbAccessObj
        # whether to query from DB or file when there is no data in the Cache, Y/N
        ReadDbFlag=Y
    </DbAccess>
    <BinLog>
        # the suffix of name of binlog file
        LogFile=binlog
        # the number of rows of the binlog that need to be synchronized each time
        MaxLine=10000
        # whether to record binlog
        Record=Y
        # whether to record key binlog
        KeyRecord=N
        # whether to use key binlog when master-slave synchronization
        KeySyncMode=N
        # whether to compress binlog when synchronizing
        SyncCompress=Y
        # whether to use gzip
        IsGzip=Y

        # the size of the cache used by SLAVE to receive the binlog
        BuffSize=10
        # interval for saving synctime file(second)
        SaveSyncTimeInterval=10
        # the period of active binlog generation (second)
        HeartbeatInterval=600
    </BinLog>
    <Record>
    </Record>
    <Router>
        ObjName=DCache.TestRouterServer.RouterObj
        PageSize=10000
        # the name of local file for saving routing table
        RouteFile=Route.dat
        # interval for synchronizing routing table (second)
        SyncInterval=1
    </Router>
</Main>
```

# Router Configuration

```
<Main>
    AppName=GuochengHelloWorld
    # Minimum interval for reloading database configuration
    DbReloadTime=300000
    # name of obj for managing interfaces
    AdminRegObj=tars.tarsregistry.AdminRegObj
    <ETCD>
        # whether to enable ETCD
        enable=N
        # Endpoints(ip:port) of all ETCD hosts,seperated by semicolons. for example: "x.x.x.x:2379;x.x.x.x:2379;x.x.x.x:2379"
        host=
        # Timeout(seconds) for requesting ETCD host
        RequestTimeout=3
        # Heartbeat timeout of Router host
        EtcdHbTTL=60
    </ETCD>
    <Transfer>
        # Maximum silence time of the proxy
        ProxyMaxSilentTime=1800
        # interval for clearing Proxies
        ClearProxyInterval=1800
        # 轮询迁移数据库的时间
        TransferInterval=3
        # number of threads used for polling
        TimerThreadSize=20
        # minimum number of threads used for migrating per module (default 5)
        MinTransferThreadEachModule=5
        # timeout waiting for migration(ms)
        TransferTimeOut=3000
        # organize the database every TransferDefragDbInterval pages when migrating
        TransferDefragDbInterval=50
        # interval for re-executing migration instructions (second)
        RetryTransferInterval=1
        # maximum number of retries when migration fails
        RetryTransferMaxTimes=3
        # number of page when migrating each time
        TransferPagesOnce=5
        # minimum number of migration threads allocated per group
        MinTransferThreadEachGroup=5
        # maximum number of migration threads allocated per group
        MaxTransferThreadEachGroup=8
    </Transfer>
    <Switch>
        # 自动切换超时的检测间隔(秒)
        SwitchCheckInterval= 10
        # timeout for automatic switching (seconds)
        SwitchTimeOut=60
        # number of threads used to perform automatic switching (default 1)
        SwitchThreadSize=50
        # slave unreadable timeout
        SlaveTimeOut=60
        # threshold (milliseconds) of the difference between the main and standby binlogs during active/standby switch
        SwitchBinLogDiffLimit=300
        # maximum number of active/standby switch in a day
        SwitchMaxTimes=3
        # time to wait master to downgrade when doing active/standby switch(seconds)
        DowngradeTimeout=30
    </Switch>
    <DB>
        <conn>
            charset=utf8
            dbname=testInstall
            dbhost=
            dbpass=
            dbport=
            dbuser=
        </conn>
        <relation>
            charset=utf8
            dbname=db_dcache_relation
            dbhost=
            dbpass=
            dbport=
            dbuser=
        </relation>
    <DB>
</Main>
```

# PropertyServer Configuration
```
<Main>
    <DB>
            Sql=CREATE TABLE `${TABLE}` (`stattime` timestamp NOT NULL default  CURRENT_TIMESTAMP,`f_date` date NOT NULL default '1970-01-01',`f_tflag` varchar(8) NOT NULL default '',`app_name` varchar(20) default NULL,`module_name` varchar(50) default NULL,`group_name` varchar(100) default NULL,`idc_area` varchar(10) default NULL,`server_status` varchar(10) default NULL,`master_name` varchar(128) NOT NULL default '',`master_ip` varchar(16) default NULL,`set_name` varchar(15) NOT NULL default '',`set_area` varchar(15) NOT NULL default '',`set_id` varchar(15) NOT NULL default  '',`value1` varchar(255) default NULL,`value2` varchar(255) default NULL,`value3` varchar(255) default NULL,`value4` varchar(255) default NULL,`value5` varchar(255) default NULL,`value6` varchar(255) default NULL,`value7` varchar(255) default NULL,`value8` varchar(255) default NULL,`value9` varchar(255) default NULL,`value10` varchar(255) default NULL,`value11` varchar(255) default NULL,`value12` varchar(255) default NULL,`value13` varchar(255) default NULL,`value14` varchar(255) default NULL,`value15` varchar(255) default NULL,`value16` varchar(255) default NULL,`value17` varchar(255) default NULL,`value18` varchar(255) default NULL,`value19` varchar(255) default NULL,`value20` varchar(255) default NULL,`value21` varchar(255) default NULL,`value22` varchar(255) default NULL,`value23` varchar(255) default NULL,`value24` varchar(255) default NULL,`value25` varchar(255) default NULL,`value26` varchar(255) default NULL,`value27` varchar(255) default NULL,`value28` varchar(255) default NULL,`value29` varchar(255) default NULL,`value30` varchar(255) default NULL,`value31` varchar(255) default NULL,`value32` varchar(255) default NULL,`value33` varchar(255) default NULL,`value34` varchar(255) default NULL,`value35` varchar(255) default NULL,KEY(`f_date`,`f_tflag`,`master_name`,`master_ip`),KEY `IDX_MASTER_NAME` (`master_name`),KEY `IDX_MASTER_IP` (`master_ip`),KEY `IDX_TIME` (`stattime`),KEY `IDX_F_DATE` (`f_date`))ENGINE\=MyISAM
            TbNamePre=t_property_realtime
            AppName=dcache_idc5min_147
        <property>
            dbhost=
            dbname=taf_property_147
            dbuser=
            dbpass=
            dbport=
            charset=gbk
        </property>
        <relation>
            charset=gbk
            dbname=db_dcache_relation
            dbhost=
            dbpass=
            dbport=
            dbuser=
        </relation>
    </DB>
    <HashMap>
        InsertInterval=1
    </HashMap>
    <NameMap>
        BakCenterError = property1
        BinLogErr = property2
        BinLogSyn = property3
        CacheError = property4
        Chunks/OnceElement = property5
        BackUpObjAdapter.connectRate = property6
        BackUpObjAdapter.queue = property7
        BinLogObjAdapter.connectRate = property8
        BinLogObjAdapter.queue = property9
        CacheObjAdapter.connectRate = property10
        CacheObjAdapter.queue = property11
        RouterClientObjAdapter.connectRate = property12
        RouterClientObjAdapter.queue = property13
        WCacheObjAdapter.connectRate = property14
        WCacheObjAdapter.queue = property15
        asyncqueue = property16
        memsize = property17
        DataMemUsedRatio = property18
        DbError = property19
        DbException = property20
        DirtyNum = property21
        DirtyRatio = property22
        ElementCount = property23
        Exception = property24
        HitCount = property25
        MemSize = property26
        getBatchCount = property27
        setBatchCount = property28
        MKMemUsedRatio = property29
        eraseCount = property30
        eraseCountUnexpire = property31
        asyncqueue0 = property32
        Jmem0DataUsedRatio = property33
        Jmem1DataUsedRatio = property34
        ColdDataRatio = property35
        expireCount = property36
        OnlyKeyCount = property37
        BigChunk = property38
    </NameMap>
</Main>

```

# ProxyServer Configuration

```
<Main>
    # Modules that need to print logs
    PrintLogModule = ModuleName1|ModuleName2
    # Specify the type of log to be printed, divided into three types: r(read), w(write), rw.
    PrintLogType = r|w
    # Indicates the idc of which region the machine where the service is depolyed belongs to
    IdcArea = sz
    # Interval for synchronizing routing table data (seconds)
    SynRouterTableInterval = 1
    # Interval to synchronize module change (new module or offline module)(seconds)
    SynRouterTableFactoryInterval = 5
    # The prefix of name of the local file storing the routing table data
    BaseLocalRouterFile = Router.dat
    # the maximum number of times to update routing table
    RouterTableMaxUpdateFrequency = 3
    # name of obj of RouterServer
    RouterObj = DCache.RouterServer.RouterObj
</Main>
```

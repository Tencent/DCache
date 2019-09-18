This document describes and examples the configuration of each module of DCache.

# ConfigServer Configuration

```
# database infomation of db_dcache_relation
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
        # interval for polling unstart transfer task in db
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
        # whether to enable auto switch
        enable = N
        # interval for checking whether need to switch (seconds)
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
        #maximum properties to monitoring
        PropertyFieldNum=48
        TbNamePre=t_property_realtime
        AppName=dcache_idc5min_147
        #database to stroe monitoring data
        <property>
            dbhost=
            dbname=db_dcache_property
            dbuser=
            dbpass=
            dbport=
            charset=gbk
        </property>
        #database of db_dcache_relation
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
        #interval to dump memory data to database(minutes)
        InsertInterval=5
    </HashMap>
    <NameMap>
        # value1 to value20 for obj and tars server fixed property
        CacheObjAdapter.queue = value1
        WCacheObjAdapter.queue = value2
        BinLogObjAdapter.queue = value3
        RouterClientObjAdapter.queue = value4
        ControlAckObjAdapter.queue = value5
        BackUpObjAdapter.queue = value6
        sendrspqueue = value7
        asyncqueue = value8
        memsize = value9
        reservedPro10 = value10
        reservedPro11 = value11
        reservedPro12 = value12
        reservedPro13 = value13
        reservedPro14 = value14
        reservedPro15 = value15
        reservedPro16 = value16
        reservedPro17 = value17
        reservedPro18 = value18
        reservedPro19 = value19
        reservedPro20 = value20

        # value21 for dcache property
        CacheMemSize_MB = value21
        DataMemUsage = value22
        CacheHitRatio = value23

        TotalCountOfRecords = value24
        ProportionOfDirtyRecords = value25
        CountOfDirtyRecords = value26
        CountOfOnlyKey = value27

        M/S_ReplicationErr = value28
        M/S_ReplicationLatency = value29
        CacheError = value30
        ProgramException = value31

        ReadRecordCount = value32
        WriteRecordCount = value33
        evictedRecordCount = value34
        evictedCountOfUnexpiredRecord = value35
        expiredRecordCount = value36

        ChunkUsedPerRecord = value37
        MaxMemUsageOfJmem = value38
        ProportionOfColdData = value39

        DbError = value40
        DbException = value41
        BackupError = value42

        MKV_MainkeyMemUsage = value43
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

# OptServer Configuration

```
<Main>
    # admin registry obj
    AdminRegObj = tars.tarsAdminRegistry.AdminRegObj
    <DB>
        <tars>
            charset = utf8
            dbname  = db_tars
            dbhost  = 
            dbport  = 
            dbuser  = 
            dbpass  = 
        </tars>
        <relation>
            charset = utf8
            dbname  = db_dcache_relation
            dbhost  = 
            dbport  = 
            dbuser  = 
            dbpass  = 
        </relation>
    </DB>
    <Release>
        ThreadCount = 5
    </Release>
    <Uninstall>
        Timeout = 20
        BakPath = /data/dcacheuninstall/
        ThreadCount = 2
    </Uninstall>
</Main>
```


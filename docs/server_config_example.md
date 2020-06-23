本文档对DCache的各服务的配置进行了说明和示例。

# ConfigServer服务配置

```
#db_dcache_relation的数据库信息
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

#
# KVCacheServer服务配置
```
<Main>
    #模块名
    ModuleName=Test

    #coredump size
    CoreSizeLimit=-1
    #备份恢复操作日志的按天日志文件名后缀
    BackupDayLog=dumpAndRecover
    #向Router上报心跳的间隔(毫秒)
    RouterHeartbeatInterval=1000
    <Cache>
        #指定共享内存使用的key
        ShmKey=12345
        #内存的大小
        ShmSize=10M
        #平均数据size，用于初始化共享内存空间
        AvgDataSize=1
        #设置hash比率(设置chunk数据块/hash项比值)
        HashRadio=2

        #是否允许淘汰数据
        EnableErase=Y
        #每次淘汰数据的时间间隔（秒）
        EraseInterval=5
        #开始淘汰数据的比率（已用chunk/所有chunk数据块*100）
        EraseRadio=95
        #淘汰线程数量
        EraseThreadCount=2
        #每次淘汰数据记录限制
        MaxEraseCountOneTime=500

        #是否启动过期清除线程
        StartExpireThread=Y
        #是否清除后端数据库
        ExpireDb=Y
        #每次清除过期数据的时间间隔（秒）
        ExpireInterval=300
        #清除频率, 0 表示不限制
        ExpireSpeed=0

        #每次回写时间间隔（秒）
        SyncInterval=300
        #回写频率, 0 表示不限制
        SyncSpeed=0
        #回写脏数据的线程数
        SyncThreadNum=1
        #回写时间（秒），即回写多久以前的数据
        SyncTime=300
        #禁止回写时间段（例：0900-1000;1600-1700）
        SyncBlockTime=0000-0000

        #是否保存OnlyKey数据 Y/N
        SaveOnlyKey=Y
        #主机自动降级超时时间(秒)，即30s无心跳则自动降级
        DowngradeTimeout=30

        #内存块个数
        JmemNum=10
        #是否统计冷数据比例
        coldDataCalEnable=Y
        #冷数据统计周期(天)
        coldDataCalCycle=7
        #后端数据库的key长度限制
        MaxKeyLengthInDB=767
    </Cache>
    <Log>
        #回写db的按天日志文件名后缀
        DbDayLog=db
    </Log>
    <DbAccess>
        #是否存在DB，Y/N
        DBFlag=Y
        #DbAccess的obj名称
        ObjName=DCache.TestDbAccessServer.DbAccessObj
        #当Cache中没有数据时，是否从DB或文件查询, Y/N
        ReadDbFlag=Y
    </DbAccess>
    <BinLog>
        #binlog日志文件名后缀
        LogFile=binlog
        #是否记录binlog
        Record=Y
        #是否记录key binlog
        KeyRecord=N
        #每次同步binlog的行数
        MaxLine=10000
        #同步binlog是否开启压缩
        SyncCompress=Y
        #采用gzip压缩格式
        IsGzip=Y
        #主备同步使用key binlog
        KeySyncMode=N

        #备机同步binlog缓存buffer的szie
        BuffSize=10
        #保存synctime文件的间隔(秒)
        SaveSyncTimeInterval=10
        #active binlog产生的周期(秒)
        HeartbeatInterval=600
    </BinLog>
    <Router>
        #RouterServer的obj名称
        ObjName=DCache.TestRouterServer.RouterObj
        #路由分页大小
        PageSize=10000
        #保存在本地的路由表文件名
        RouteFile=Route.dat
        #同步路由的时间间隔（秒）
        SyncInterval=1
    </Router>
</Main>
```
# MKVCacheServer服务配置
```
<Main>
    #模块名
    ModuleName=Test

    #coredump size
    CoreSizeLimit=-1
    #限制按主key查询最大数据条数
    MKeyMaxBlockCount=20000
    #备份恢复操作日志的按天日志文件名后缀
    BackupDayLog=dumpAndRecover
    #向Router上报心跳的间隔(毫秒)
    RouterHeartbeatInterval=1000
    <Cache>
        #指定共享内存使用的key
        ShmKey=692815670
        #内存的大小
        ShmSize=10M
        #平均数据size，用于初始化共享内存空间
        AvgDataSize=1
        #设置hash比率(设置chunk数据块/hash项比值)
        HashRadio=2
        #元素个数与主key个数的比率
        MKHashRadio=1
        #存储数据结构类型，hash/set/zset/list
        MainKeyType=hash

        #是否启动删除线程
        StartDeleteThread=Y
        #delete线程操作时间间隔（秒）
        DeleteInterval=300
        #delete线程删除速度，0表示不限速
        DeleteSpeed=0

        #是否允许淘汰数据
        EnableErase=Y
        #每次淘汰数据的时间间隔（秒）
        EraseInterval=5
        #开始淘汰数据的比率（已用chunk/所有chunk数据块*100）
        EraseRadio=95
        #淘汰线程数量
        EraseThreadCount=2
        #每次淘汰数据记录限制
        MaxEraseCountOneTime=500

        #是否启动过期清除线程
        StartExpireThread=Y
        #是否清除后端数据库
        ExpireDb=N
        #每次清除过期数据的时间间隔（秒）
        ExpireInterval=3600
        #清除频率, 0 表示不限制
        ExpireSpeed=0

        #每次回写时间间隔（秒）
        SyncInterval=300
        #回写频率, 0 表示不限制
        SyncSpeed=0
        #回写脏数据的线程数
        SyncThreadNum=1
        #回写时间（秒），即回写多久以前的数据
        SyncTime=300
        #屏蔽回写时间段（例：0900-1000;1600-1700）
        SyncBlockTime=0000-0000
        #解除屏蔽回写的脏数据比率
        SyncUNBlockPercent=60

        #insert一个数据是在主key链的头部还是尾部，Y/N -- 头/尾
        InsertOrder=N
        #数据更新时，同时更新主key链下的顺序
        UpdateOrder=N
        #主key最大记录条数限制，只在无源cache时有效，超过最大记录限制将删除旧的数据，0表示无限制
        MkeyMaxDataCount=0
        #是否保存OnlyKey数据 Y/N
        SaveOnlyKey=Y
        #从DB中查回的数据，写入cache时按照OrderItem指定的字段排序插入主key链
        OrderItem=
        #按照OrderItem排序时，是否采用降序
        OrderDesc=Y

        #内存块个数
        JmemNum=2
        #主机自动降级超时时间(秒)，即30s无心跳则自动降级
        DowngradeTimeout=30
        #是否统计冷数据比例
        coldDataCalEnable=Y
        #冷数据统计周期(天)
        coldDataCalCycle=7
        #后端数据库的key长度限制
        MaxKeyLengthInDB=767
        #数据迁移时传输数据进行压缩
        transferCompress=Y
    </Cache>
    <Log>
        #回写db的按天日志文件名后缀
        DbDayLog=db
    </Log>
    <DbAccess>
        #是否存在DB，Y/N
        DBFlag=Y
        #DbAccess的obj名称
        ObjName=DCache.TestDbAccessServer.DbAccessObj
        #当Cache中没有数据时，是否从DB或文件查询, Y/N
        ReadDbFlag=Y
    </DbAccess>
    <BinLog>
        #binlog日志文件名后缀
        LogFile=binlog
        #每次同步binlog的行数
        MaxLine=10000
        #是否记录binlog
        Record=Y
        #是否记录key binlog
        KeyRecord=N
        #主备同步使用key binlog
        KeySyncMode=N
        #同步binlog是否开启压缩
        SyncCompress=Y
        #采用gzip压缩格式
        IsGzip=Y

        #备机同步binlog缓存buffer的szie
        BuffSize=10
        #保存synctime文件的间隔(秒)
        SaveSyncTimeInterval=10
        #active binlog产生的周期(秒)
        HeartbeatInterval=600
    </BinLog>
    #用户字段信息，模块接入时自动生成
    <Record>
    </Record>
    <Router>
        #RouterServer的obj名称
        ObjName=DCache.TestRouterServer.RouterObj
        #路由分页大小
        PageSize=10000
        #保存在本地的路由表文件名
        RouteFile=Route.dat
        #同步路由的时间间隔（秒）
        SyncInterval=1
    </Router>
</Main>
```

# Router服务配置
```
<Main>
    # 应用名称
    AppName=GuochengHelloWorld
    # 数据库配置重新装载最小间隔时间
    DbReloadTime=300000
    # 管理接口的Obj
    AdminRegObj=tars.tarsregistry.AdminRegObj
    <ETCD>
        # 是否开启ETCD为Router集群(Router集群需要利用ETCD来做选举)
        enable=N
        # 所有ETCD机器的地址，以分号分割，如x.x.x.x:2379;x.x.x.x:2379;x.x.x.x:2379
        host=
        # ETCD通信请求的超时时间(秒)
        RequestTimeout=3
        # Router主机心跳的维持时间(秒)
        EtcdHbTTL=60
    </ETCD>
    <Transfer>
        # 清理代理的最近未访问时间
        ProxyMaxSilentTime=1800
        # 清理代理的间隔时间
        ClearProxyInterval=1800
        # 轮询迁移数据库的时间
        TransferInterval=3
        # 轮询线程数
        TimerThreadSize=20
        # 每个模块最小迁移线程数(没有这项配置的话，默认是5个线程)
        MinTransferThreadEachModule=5
        # 等待页迁移的超时时间(毫秒)
        TransferTimeOut=3000
        # 迁移时隔多少页整理一下数据库记录
        TransferDefragDbInterval=50
        # 重新执行迁移指令的时间间隔(秒)
        RetryTransferInterval=1
        # 迁移失败时的最大重试次数
        RetryTransferMaxTimes=3
        # 一次迁移页数
        TransferPagesOnce=5
        # 每个组分配的最小迁移线程数
        MinTransferThreadEachGroup=5
        # 每个组分配的最大迁移线程数
        MaxTransferThreadEachGroup=8
    </Transfer>
    <Switch>
        # 是否开启Cache主备自动切换
        enable = N
        # 自动切换超时的检测间隔(秒)
        SwitchCheckInterval= 10
        # 自动切换的超时时间(秒)
        SwitchTimeOut=60
        # 自动切换执行的线程数(默认1个)
        SwitchThreadSize=50
        # 备机不可读的超时时间(秒)
        SlaveTimeOut=60
        # 主备切换时，主备机binlog差异的阈值(毫秒)
        SwitchBinLogDiffLimit=300
        # 一天当中主备切换的最大次数
        SwitchMaxTimes=3
        # 主备切换时等待主机降级的时间(秒)
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

# PropertyServer服务配置
```
<Main>
    <DB>
        #可监控属性数量的最大值
        PropertyFieldNum=48
        TbNamePre=t_property_realtime
        AppName=dcache_idc5min_147
        #特性监控数据库信息
        <property>
            dbhost=
            dbname=db_dcache_property
            dbuser=
            dbpass=
            dbport=
            charset=utf8
        </property>
        #db_dcache_relation的数据库信息
        <relation>
            charset=utf8
            dbname=db_dcache_relation
            dbhost=
            dbpass=
            dbport=
            dbuser=
        </relation>
    </DB>
    <HashMap>
        #数据入库的时间间隔，单位分钟
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

# ProxyServer服务配置

```
<Main>
    PrintLogModule = ModuleName1|ModuleName2 #需要打印日志的模块
    PrintLogType = r|w #指定打印的日志类型，分三种：仅读打印，仅写打印，读写都打印
    IdcArea = sz #指明服务部署的机器属于那个地区idc
    SynRouterTableInterval = 1 #同步路由表的时间间隔（秒）
    SynRouterTableFactoryInterval = 5 #同步模块变化（模块新增或者下线）的时间间隔（秒）
    BaseLocalRouterFile = Router.dat  #保存在本地的路由表文件名前缀
    RouterTableMaxUpdateFrequency = 3 #模块路由表每秒钟的最大更新频率
    RouterObj = DCache.RouterServer.RouterObj #RouterServer的obj名称
</Main>
```

# OptServer服务配置

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
        # 发布服务线程数
        ThreadCount = 5
    </Release>
    <Uninstall>
        # 通知node下线服务超时时间(秒)
        Timeout = 20
        # 下线服务备份目录
        BakPath = /data/dcacheuninstall/
        # 下线服务线程数
        ThreadCount = 2
    </Uninstall>
</Main>
```
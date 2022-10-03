const Sequelize = require('sequelize');

const DbUpdate = {};

DbUpdate.update = async (dbRelation) => {

    // console.log("update", dbRelation);

    try {
        // console.log("update");
        let data = await dbRelation.tConfigItem.findOne({
            where: {
                id: 477
            }
        });

        // console.log(data);
        //如果没有数据, 则初始化
        if (!data) {

            let configItem = [];
            configItem.push([477, 'Cache模块名', 'ModuleName', 'Main', '1', 'U']);
            configItem.push([479, 'RouterServer的obj名称', 'ObjName', 'Main/BinLog', '0', 'U']);
            configItem.push([480, '路由分页大小', 'PageSize', 'Main/Router', '1', 'U']);
            configItem.push([481, '同步路由的时间间隔(秒)', 'SynInterval', 'Main/BinLog', '0', 'U']);
            configItem.push([482, '保存在本地路由表的文件名', 'RouteFile', 'Main/Router', '0', 'U']);
            configItem.push([484, '主key数据块大小', 'MKSize', 'Main/Cache', '0', 'U']);
            configItem.push([485, '最小Cache数据块大小', 'MinDataSize', 'Main/Cache', '0', 'U']);
            configItem.push([486, '最大Cache数据块大小', 'MaxDataSize', 'Main/Cache', '0', 'U']);
            configItem.push([487, 'Cache数据块的增长因子', 'Factor', 'Main/Cache', '0', 'U']);
            configItem.push([488, '主键hash比率', 'HashRadio', 'Main/Cache', '0', 'U']);
            configItem.push([489, '元素个数与主key个数的比率', 'MKHashRadio', 'Main/Cache', '0', 'U']);
            configItem.push([490, '使能容量淘汰', 'EnableErase', 'Main/Cache', '1', 'U']);
            configItem.push([491, 'mmap文件名', 'CacheFile', 'Main/Cache', '0', 'U']);
            configItem.push([492, 'Cache已分配的内存大小', 'ShmSize', 'Main/Cache', '0', 'U']);
            configItem.push([493, '回写时间(秒)，即回写多久以前的数据，时间越大，保留脏数据越多', 'SyncTime', 'Main/Cache', '1', 'U']);
            configItem.push([494, '回写操作的时间间隔(秒)，即频率', 'SyncInterval', 'Main/Cache', '1', 'U']);
            configItem.push([495, '淘汰数据的时间间隔(秒)', 'EraseInterval', 'Main/Cache', '1', 'U']);
            configItem.push([496, '回写脏数据的线程数，线程越大，回写越快，DB压力越大', 'SyncThreadNum', 'Main/Cache', '1', 'U']);
            configItem.push([497, '回写速度(秒), 0 表示不限制', 'SyncSpeed', 'Main/Cache', '1', 'U']);
            configItem.push([498, '是否保存OnlyKey数据 Y/N', 'SaveOnlyKey', 'Main/Cache', '0', 'U']);
            configItem.push([499, 'insert一个数据是在主key链表的头部还是尾部，Y/N -- 头/尾', 'InsertOrder', 'Main/Cache', '1', 'U']);
            configItem.push([500, '是否启动过期数据自动清除线程', 'StartExpireThread', 'Main/Cache', '1', 'U']);
            configItem.push([501, '清除过期数据的时间间隔(秒)', 'ExpireInterval', 'Main/Cache', '0', 'U']);
            configItem.push([502, '过期清理是否同时清理数据库数据', 'ExpireDb', 'Main/Cache', '0', 'U']);
            configItem.push([503, '清除频率，限制单次清除的数据个数， 0 表示不限制', 'ExpireSpeed', 'Main/Cache', '0', 'U']);
            configItem.push([504, 'binlog日志文件名后缀', 'LogFile', 'Main/BinLog', '1', 'U']);
            configItem.push([505, '每次同步binlog的行数', 'MaxLine', 'Main/BinLog', '1', 'U']);
            configItem.push([506, '是否同步写binlog', 'WriteSync', 'Main/BinLog', '0', 'U']);
            configItem.push([507, '是否记录binlog，Y/N(支持大小写) -- 是/否', 'Record', 'Main/BinLog', '1', 'U']);
            configItem.push([508, '是否存在DB或文件存储，Y/N', 'DBFlag', 'Main/DbAccess', '1', 'U']);
            configItem.push([509, '当Cache中没有数据时，是否从DB或文件查询, Y/N', 'ReadDbFlag', 'Main/DbAccess', '0', 'U']);
            configItem.push([510, 'DbAccess的obj名称', 'ObjName', 'Main/DbAccess', '1', 'U']);
            configItem.push([511, '调用DbAccessServer的接口类型，Byte/String Byte表示vector类型', 'IntfaceType', 'Main/DbAccess', '1', 'U']);
            configItem.push([512, '二期主key', 'MKey', 'Main/Record', '0', 'MK']);
            configItem.push([513, '二期联合key', 'UKey', 'Main/Record', '0', 'MK']);
            configItem.push([514, '二期数据字段', 'VKey', 'Main/Record', '0', 'MK']);
            configItem.push([515, '同步路由的时间间隔(秒)', 'SyncInterval', 'Main/Router', '1', 'U']);
            configItem.push([516, '开始淘汰数据的比率(已用chunk/所有chunk数据块*100)', 'EraseRadio', 'Main/Cache', '1', 'U']);
            configItem.push([517, '是否写远程binlog，Y/N(支持大小写)', 'Remote', 'Main/BinLog', '0', 'U']);
            configItem.push([518, '是否使用自动压缩binlog接口，Y/N', 'SyncCompress', 'Main/BinLog', '1', 'U']);
            configItem.push([519, '共享内存key(10进制)', 'ShmKey', 'Main/Cache', '0', 'U']);
            configItem.push([520, 'NULL', 'SemKey', 'Main/Cache', '0', 'U']);
            configItem.push([523, '限制core size(-1为不限制)', 'CoreSizeLimit', 'Main', '0', 'U']);
            configItem.push([524, 'RouterServer的obj名称', 'ObjName', 'Main/Router', '0', 'U']);
            configItem.push([525, '主key最大记录条数限制，只在无源cache时有效，超过最大记录数据将删除旧的数据', 'MkeyMaxDataCount', 'Main/Cache', '0', 'U']);
            configItem.push([526, '', 'IndexFile', 'Main/Cache', '1', 'U']);
            configItem.push([527, '虚拟的配置项，用来存MKey,UKey,VKey的值', 'VirtualRecord', 'Main/Record/Field', '0', 'MK']);
            configItem.push([528, '回写db的按天日志文件名后缀', 'DbDayLog', 'Main/Log', '1', 'U']);
            configItem.push([529, '备份中心服务obj', 'ObjName', 'Main/BakCenter', '0', 'U']);
            configItem.push([530, '访问配置中心的线程线', 'ReadThreadNum', 'Main/BakCenter', '0', 'U']);
            configItem.push([531, '内存结构优化chunk自动分配大小参数项，配置之后，原有chunk大小配置无效', 'AvgDataSize', 'Main/Cache', '0', 'U']);
            configItem.push([532, '过期时间是否落地DB', 'ExpireTimeInDB', 'Main', '1', 'MK']);
            configItem.push([533, '数据恢复同步数据线程数', 'ReadThreadNum', 'Main/BackUpClient', '0', 'U']);
            configItem.push([534, '数据库迁移期间屏蔽所有更新DB操作', 'DBMigration', 'Main/DbAccess', '0', 'U']);
            configItem.push([535, '是否写keybinlog,Y/N', 'KeyRecord', 'Main/BinLog', '0', 'U']);
            configItem.push([536, '主备机同步方式是否通过key,Y/N', 'KeySyncMode', 'Main/BinLog', '0', 'T']);
            configItem.push([537, '是否使用GZIP压缩binlog进行传输,Y/N', 'IsGzip', 'Main/BinLog', '0', 'T']);
            configItem.push([538, '是否启用binlog同步线程', 'StartBinlogThread', 'Main/Cache', '0', 'U']);
            configItem.push([539, '限制按主key查询最大数据条数，默认5W', 'MKeyMaxBlockCount', 'Main', '0', 'MK']);
            configItem.push([540, '热点分析访问日志', 'AnalyseLog', 'Main', '0', 'U']);
            configItem.push([541, '淘汰线程数', 'EraseThreadCount', 'Main/Cache', '1', 'U']);
            configItem.push([542, 'del线程操作时间间隔(秒)', 'DeleteInterval', 'Main/Cache', '300', 'MK']);
            configItem.push([543, 'del线程删除速度，0表示不限速', 'DeleteSpeed', 'Main/Cache', '0', 'MK']);
            configItem.push([544, '是否启动del线程', 'StartDeleteThread', 'Main/Cache', 'Y', 'MK']);
            configItem.push([545, '解除屏蔽回写的脏数据比率', 'SyncUNBlockPercent', 'Main/Cache', '60', 'U']);
            configItem.push([546, '屏蔽回写时间间隔(eg: 0900-1000;1600-1700)', 'SyncBlockTime', 'Main/Cache', '0', 'U']);
            configItem.push([547, 'key最大长度', 'MaxKeyLengthInDB', 'Main/Cache', '767', 'MK']);
            configItem.push([548, '更新主key下的顺序', 'UpdateOrder', 'Main/Cache', 'N', 'MK']);
            configItem.push([549, '主key数据类型(hash,list,set,zset)', 'MainKeyType', 'Main/Cache', 'hash', 'MK']);
            configItem.push([550, '是否进行热点数据统计', 'coldDataCalEnable', 'Main/Cache', '0', 'U']);
            configItem.push([551, '热点数据重置周期', 'coldDataCalCycle', 'Main/Cache', '0', 'U']);
            configItem.push([552, '查询耗时统计开关', 'SelectTimeStat', 'Main', 'Y', '0']);
            configItem.push([553, '查询耗时统计阈值', 'TimeStatMin', 'Main', '3', '0']);
            configItem.push([554, 'DB返回数据排序字段名', 'OrderItem', 'Main/Cache', '0', 'MK']);
            configItem.push([555, '强一致主机处理ack启用线程池', 'SyncMode', 'Main/Consistency', '1', '1']);
            configItem.push([556, '强一致模式下，定时ack间隔', 'AckIntervalMs', 'Main/BinLog', '1', '1']);
            configItem.push([557, '主机自动降级的超时时间', 'DowngradeTimeout', 'Main/Cache', '1', '1']);
            configItem.push([558, '应用名', 'AppName', 'Main', '1', '1']);
            configItem.push([559, 'etcd请求超时时间', 'etcdReqTimeout', 'Main/etcd', '1', '1']);
            configItem.push([560, 'etcd监听超时时间', 'etcdWatchTimeout', 'Main/etcd', '1', '1']);
            configItem.push([561, 'cache心跳超时时间', 'CacheHbTimeout', 'Main/etcd', '1', '1']);
            configItem.push([562, 'etcd的节点ip和port', 'host', 'Main/etcd', '1', '1']);
            configItem.push([563, '强一致耗时统计开关', 'TimeStatisForWrite', 'Main', '1', '1']);
            configItem.push([564, '强一致主机处理ack启用线程池', 'AckPoolEnable', 'Main/Consistency', '1', '1']);
            configItem.push([565, '强一致同步binlog线程数', 'ConsReqThreadNum', 'Main/BinLog', '1', '1']);
            configItem.push([566, '强一致是否多线程同步binlog', 'ConsMultiPull', 'Main/BinLog', '1', '1']);
            configItem.push([567, '内存块个数', 'JmemNum', 'Main/Cache', '2', 'U']);
            configItem.push([568, 'binlog落地阈值(M)', 'BinLogBufferLandSize', 'Main/BinLog', '100', '1']);
            configItem.push([569, 'binlog落地时间', 'BinLogBufferLandTime', 'Main/BinLog', '10', '1']);
            configItem.push([570, 'binlog优化缓存大小', 'BinLogBufferSize', 'Main/BinLog', '100', '1']);
            configItem.push([571, '开启binlog缓存优化', 'openBinLogBuffer', 'Main/BinLog', 'N', '1']);
            configItem.push([572, '脏数据动态回写能力', 'DynamicSyncThread', 'Main/Cache', 'N', 'U']);
            configItem.push([573, 'binlog共享内存key', 'BinlogShmKey', 'Main/BinLog', '0', '1']);
            configItem.push([574, 'bitmap分块个数', 'BitMapBlockCount', 'Main', '8', 'U']);
            configItem.push([575, 'Cache数据的value类型: string/bitmap', 'ValueType', 'Main/Cache', '0', 'U']);
            configItem.push([604, '备份恢复操作日志的按天日志文件名后缀', 'BackupDayLog', 'Main', '0', 'U']);
            configItem.push([605, '向Router上报心跳的间隔(毫秒)', 'RouterHeartbeatInterval', 'Main', '0', 'U']);
            configItem.push([608, '每次淘汰数据记录限制', 'MaxEraseCountOneTime', 'Main/Cache', '500', 'U']);
            configItem.push([609, '备机同步binlog缓存buffer的szie', 'BuffSize', 'Main/BinLog', '0', 'U']);
            configItem.push([610, '保存synctime文件的间隔(秒)', 'SaveSyncTimeInterval', 'Main/BinLog', '0', 'U']);
            configItem.push([611, 'active binlog产生的周期(秒)', 'HeartbeatInterval', 'Main/BinLog', '0', 'U']);
            configItem.push([612, '按照OrderItem排序时，是否采用降序 Y/N', 'OrderDesc', 'Main/Cache', '0', 'U']);
            configItem.push([613, '数据迁移时传输数据进行压缩', 'transferCompress', 'Main/Cache', '0', 'U']);

            for (let i = 0; i < configItem.length; i++) {
                let param = configItem[i];

                await dbRelation.tConfigItem.create({
                    id: param[0],
                    remark: param[1],
                    item: param[2],
                    path: param[3],
                    reload: param[4],
                    period: param[5],
                });
            }
        }

    } catch (e) {
        console.log("update error", e);

    }
}


module.exports = DbUpdate;


-- MySQL dump 10.13  Distrib 5.6.26, for Linux (x86_64)
--
-- Host: localhost    Database: db_tars
-- ------------------------------------------------------
-- Server version	5.6.26-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `t_router_app`
--

DROP TABLE IF EXISTS `t_router_app`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_router_app` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `router_name` varchar(128) NOT NULL DEFAULT '',
  `app_name` varchar(128) NOT NULL DEFAULT '',
  `host` char(50) DEFAULT NULL,
  `port` char(10) DEFAULT NULL,
  `user` char(50) DEFAULT NULL,
  `password` char(50) DEFAULT NULL,
  `charset` char(10) DEFAULT 'utf8mb4',
  `db_name` char(50) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `router_app` (`router_name`,`app_name`)
) ENGINE=InnoDB AUTO_INCREMENT=1528 DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `t_proxy_app`
--

DROP TABLE IF EXISTS `t_proxy_app`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_proxy_app` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `proxy_name` varchar(128) NOT NULL DEFAULT '',
  `app_name` varchar(128) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  UNIQUE KEY `proxy_app` (`proxy_name`,`app_name`)
) ENGINE=InnoDB AUTO_INCREMENT=587 DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_proxy_router`
--

DROP TABLE IF EXISTS `t_proxy_router`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_proxy_router` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `proxy_name` varchar(128) NOT NULL DEFAULT '',
  `router_name` varchar(128) NOT NULL DEFAULT '',
  `db_name` varchar(64) NOT NULL DEFAULT '',
  `db_ip` varchar(64) NOT NULL DEFAULT '',
  `modulename_list` tinytext NOT NULL,
  `modify_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `proxy_router` (`proxy_name`,`router_name`)
) ENGINE=InnoDB AUTO_INCREMENT=2443935 DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `t_config_appMod`
--

DROP TABLE IF EXISTS `t_config_appMod`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_config_appMod` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `appName` varchar(128) DEFAULT NULL,
  `moduleName` varchar(128) DEFAULT NULL,
  `cacheType` varchar(16) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `app_module` (`appName`,`moduleName`)
) ENGINE=InnoDB AUTO_INCREMENT=42270 DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `t_cache_router`
--

DROP TABLE IF EXISTS `t_cache_router`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_cache_router` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `cache_name` varchar(128) NOT NULL DEFAULT '',
  `cache_ip` varchar(30) NOT NULL DEFAULT '',
  `router_name` varchar(128) NOT NULL DEFAULT '',
  `db_name` varchar(64) NOT NULL DEFAULT '',
  `db_ip` varchar(64) NOT NULL DEFAULT '',
  `db_port` varchar(8) NOT NULL DEFAULT '',
  `db_user` varchar(64) NOT NULL DEFAULT '',
  `db_pwd` varchar(64) NOT NULL DEFAULT '',
  `db_charset` varchar(8) NOT NULL DEFAULT '',
  `module_name` varchar(64) NOT NULL DEFAULT '',
  `app_name` varchar(80) NOT NULL DEFAULT '',
  `idc_area` varchar(10) NOT NULL DEFAULT '',
  `group_name` varchar(128) NOT NULL DEFAULT '',
  `server_status` char(1) NOT NULL DEFAULT '',
  `priority` tinyint(4) NOT NULL DEFAULT '1',
  `modify_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `mem_size` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `cache_name` (`cache_name`),
  KEY `app_name` (`app_name`),
  KEY `module_name` (`module_name`),
  KEY `cache_ip` (`cache_ip`),
  KEY `group_name` (`group_name`),
  KEY `server_status` (`server_status`),
  KEY `idc_area` (`idc_area`)
) ENGINE=InnoDB AUTO_INCREMENT=13869505 DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `t_proxy_cache`
--

DROP TABLE IF EXISTS `t_proxy_cache`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_proxy_cache` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `proxy_name` varchar(128) NOT NULL DEFAULT '',
  `cache_name` varchar(128) NOT NULL DEFAULT '',
  `modify_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  UNIQUE KEY `proxy_cache` (`proxy_name`,`cache_name`)
) ENGINE=InnoDB AUTO_INCREMENT=390163667 DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `t_config_table`
--

DROP TABLE IF EXISTS `t_config_table`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_config_table` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `config_id` int(11) NOT NULL DEFAULT '0',
  `server_name` varchar(128) DEFAULT NULL,
  `host` varchar(20) DEFAULT NULL,
  `item_id` int(11) NOT NULL DEFAULT '0',
  `config_value` text,
  `level` int(11) DEFAULT NULL,
  `posttime` datetime DEFAULT NULL,
  `lastuser` varchar(50) DEFAULT NULL,
  `config_flag` int(10) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `config_item_new` (`config_id`,`item_id`,`server_name`),
  KEY `item` (`item_id`),
  KEY `server` (`server_name`,`host`),
  KEY `item_id` (`item_id`)
) ENGINE=InnoDB AUTO_INCREMENT=1308101 DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `t_config_reference`
--

DROP TABLE IF EXISTS `t_config_reference`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_config_reference` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `reference_id` int(11) DEFAULT NULL,
  `server_name` varchar(128) DEFAULT NULL,
  `host` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `reference_id` (`server_name`,`host`)
) ENGINE=InnoDB AUTO_INCREMENT=194653 DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `t_config_item`
--

DROP TABLE IF EXISTS `t_config_item`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_config_item` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `remark` varchar(128) DEFAULT NULL,
  `item` varchar(128) DEFAULT NULL,
  `path` varchar(128) DEFAULT NULL,
  `reload` varchar(4) DEFAULT NULL,
  `period` varchar(4) DEFAULT 'U',
  PRIMARY KEY (`id`),
  UNIQUE KEY `item_path` (`item`,`path`)
) ENGINE=InnoDB AUTO_INCREMENT=350 DEFAULT CHARSET=utf8mb4;

--
-- Dumping data for table `t_config_item`
--

LOCK TABLES `t_config_item` WRITE;
/*!40000 ALTER TABLE `t_config_item` DISABLE KEYS */;
INSERT INTO `t_config_item` VALUES (477,'Cache模块名','ModuleName','Main','1','U'),(479,'RouterServer的obj名称','ObjName','Main/BinLog','0','U'),(480,'路由分页大小','PageSize','Main/Router','1','U'),(481,'同步路由的时间间隔(秒)','SynInterval','Main/BinLog','0','U'),(482,'保存在本地路由表的文件名','RouteFile','Main/Router','0','U'),(484,'主key数据块大小','MKSize','Main/Cache','0','U'),(485,'最小Cache数据块大小','MinDataSize','Main/Cache','0','U'),(486,'最大Cache数据块大小','MaxDataSize','Main/Cache','0','U'),(487,'Cache数据块的增长因子','Factor','Main/Cache','0','U'),(488,'主键hash比率','HashRadio','Main/Cache','0','U'),(489,'元素个数与主key个数的比率','MKHashRadio','Main/Cache','0','U'),(490,'使能容量淘汰','EnableErase','Main/Cache','1','U'),(491,'mmap文件名','CacheFile','Main/Cache','0','U'),(492,'Cache已分配的内存大小','ShmSize','Main/Cache','0','U'),(493,'回写时间(秒)，即回写多久以前的数据，时间越大，保留脏数据越多','SyncTime','Main/Cache','1','U'),(494,'回写操作的时间间隔(秒)，即频率','SyncInterval','Main/Cache','1','U'),(495,'淘汰数据的时间间隔(秒)','EraseInterval','Main/Cache','1','U'),(496,'回写脏数据的线程数，线程越大，回写越快，DB压力越大','SyncThreadNum','Main/Cache','1','U'),(497,'回写速度(秒), 0 表示不限制','SyncSpeed','Main/Cache','1','U'),(498,'是否保存OnlyKey数据 Y/N','SaveOnlyKey','Main/Cache','0','U'),(499,'insert一个数据是在主key链表的头部还是尾部，Y/N -- 头/尾','InsertOrder','Main/Cache','1','U'),(500,'是否启动过期数据自动清除线程','StartExpireThread','Main/Cache','1','U'),(501,'清除过期数据的时间间隔(秒)','ExpireInterval','Main/Cache','0','U'),(502,'过期清理是否同时清理数据库数据','ExpireDb','Main/Cache','0','U'),(503,'清除频率，限制单次清除的数据个数， 0 表示不限制','ExpireSpeed','Main/Cache','0','U'),(504,'binlog日志文件名后缀','LogFile','Main/BinLog','1','U'),(505,'每次同步binlog的行数','MaxLine','Main/BinLog','1','U'),(506,'是否同步写binlog','WriteSync','Main/BinLog','0','U'),(507,'是否记录binlog，Y/N(支持大小写) -- 是/否','Record','Main/BinLog','1','U'),(508,'是否存在DB或文件存储，Y/N','DBFlag','Main/DbAccess','1','U'),(509,'当Cache中没有数据时，是否从DB或文件查询, Y/N','ReadDbFlag','Main/DbAccess','0','U'),(510,'DbAccess的obj名称','ObjName','Main/DbAccess','1','U'),(511,'调用DbAccessServer的接口类型，Byte/String Byte表示vector类型','IntfaceType','Main/DbAccess','1','U'),(512,'二期主key','MKey','Main/Record','0','MK'),(513,'二期联合key','UKey','Main/Record','0','MK'),(514,'二期数据字段','VKey','Main/Record','0','MK'),(515,'同步路由的时间间隔(秒)','SyncInterval','Main/Router','1','U'),(516,'开始淘汰数据的比率(已用chunk/所有chunk数据块*100)','EraseRadio','Main/Cache','1','U'),(517,'是否写远程binlog，Y/N(支持大小写)','Remote','Main/BinLog','0','U'),(518,'是否使用自动压缩binlog接口，Y/N','SyncCompress','Main/BinLog','1','U'),(519,'共享内存key(10进制)','ShmKey','Main/Cache','0','U'),(520,'NULL','SemKey','Main/Cache','0','U'),(523,'限制core size(-1为不限制)','CoreSizeLimit','Main','0','U'),(524,'RouterServer的obj名称','ObjName','Main/Router','0','U'),(525,'主key最大记录条数限制，只在无源cache时有效，超过最大记录数据将删除旧的数据','MkeyMaxDataCount','Main/Cache','0','U'),(526,'','IndexFile','Main/Cache','1','U'),(527,'虚拟的配置项，用来存MKey,UKey,VKey的值','VirtualRecord','Main/Record/Field','0','MK'),(528,'回写db的按天日志文件名后缀','DbDayLog','Main/Log','1','U'),(529,'备份中心服务obj','ObjName','Main/BakCenter','0','U'),(530,'访问配置中心的线程线','ReadThreadNum','Main/BakCenter','0','U'),(531,'内存结构优化chunk自动分配大小参数项，配置之后，原有chunk大小配置无效','AvgDataSize','Main/Cache','0','U'),(532,'过期时间是否落地DB','ExpireTimeInDB','Main','1','MK'),(533,'数据恢复同步数据线程数','ReadThreadNum','Main/BackUpClient','0','U'),(534,'数据库迁移期间屏蔽所有更新DB操作','DBMigration','Main/DbAccess','0','U'),(535,'是否写keybinlog,Y/N','KeyRecord','Main/BinLog','0','U'),(536,'主备机同步方式是否通过key,Y/N','KeySyncMode','Main/BinLog','0','T'),(537,'是否使用GZIP压缩binlog进行传输,Y/N','IsGzip','Main/BinLog','0','T'),(538,'是否启用binlog同步线程','StartBinlogThread','Main/Cache','0','U'),(539,'限制按主key查询最大数据条数，默认5W','MKeyMaxBlockCount','Main','0','MK'),(540,'热点分析访问日志','AnalyseLog','Main','0','U'),(541,'淘汰线程数','EraseThreadCount','Main/Cache','1','U'),(542,'del线程操作时间间隔(秒)','DeleteInterval','Main/Cache','300','MK'),(543,'del线程删除速度，0表示不限速','DeleteSpeed','Main/Cache','0','MK'),(544,'是否启动del线程','StartDeleteThread','Main/Cache','Y','MK'),(545,'解除屏蔽回写的脏数据比率','SyncUNBlockPercent','Main/Cache','60','U'),(546,'屏蔽回写时间间隔(eg: 0900-1000;1600-1700)','SyncBlockTime','Main/Cache','0','U'),(547,'key最大长度','MaxKeyLengthInDB','Main/Cache','767','MK'),(548,'更新主key下的顺序','UpdateOrder','Main/Cache','N','MK'),(549,'主key数据类型(hash,list,set,zset)','MainKeyType','Main/Cache','hash','MK'),(550,'是否进行热点数据统计','coldDataCalEnable','Main/Cache','0','U'),(551,'热点数据重置周期','coldDataCalCycle','Main/Cache','0','U'),(552,'查询耗时统计开关','SelectTimeStat','Main','Y','0'),(553,'查询耗时统计阈值','TimeStatMin','Main','3','0'),(554,'DB返回数据排序字段名','OrderItem','Main/Cache','0','MK'),(555,'强一致主机处理ack启用线程池','SyncMode','Main/Consistency','1','1'),(556,'强一致模式下，定时ack间隔','AckIntervalMs','Main/BinLog','1','1'),(557,'主机自动降级的超时时间','DowngradeTimeout','Main/Cache','1','1'),(558,'应用名','AppName','Main','1','1'),(559,'etcd请求超时时间','etcdReqTimeout','Main/etcd','1','1'),(560,'etcd监听超时时间','etcdWatchTimeout','Main/etcd','1','1'),(561,'cache心跳超时时间','CacheHbTimeout','Main/etcd','1','1'),(562,'etcd的节点ip和port','host','Main/etcd','1','1'),(563,'强一致耗时统计开关','TimeStatisForWrite','Main','1','1'),(564,'强一致主机处理ack启用线程池','AckPoolEnable','Main/Consistency','1','1'),(565,'强一致同步binlog线程数','ConsReqThreadNum','Main/BinLog','1','1'),(566,'强一致是否多线程同步binlog','ConsMultiPull','Main/BinLog','1','1'),(567,'内存块个数','JmemNum','Main/Cache','2','U'),(568,'binlog落地阈值(M)','BinLogBufferLandSize','Main/BinLog','100','1'),(569,'binlog落地时间','BinLogBufferLandTime','Main/BinLog','10','1'),(570,'binlog优化缓存大小','BinLogBufferSize','Main/BinLog','100','1'),(571,'开启binlog缓存优化','openBinLogBuffer','Main/BinLog','N','1'),(572,'脏数据动态回写能力','DynamicSyncThread','Main/Cache','N','U'),(573,'binlog共享内存key','BinlogShmKey','Main/BinLog','0','1'),(574,'bitmap分块个数','BitMapBlockCount','Main','8','U'),(575,'Cache数据的value类型: string/bitmap','ValueType','Main/Cache','0','U'),(604,'备份恢复操作日志的按天日志文件名后缀','BackupDayLog','Main','0','U'),(605,'向Router上报心跳的间隔(毫秒)','RouterHeartbeatInterval','Main','0','U'),(608,'每次淘汰数据记录限制','MaxEraseCountOneTime','Main/Cache','500','U'),(609,'备机同步binlog缓存buffer的szie','BuffSize','Main/BinLog','0','U'),(610,'保存synctime文件的间隔(秒)','SaveSyncTimeInterval','Main/BinLog','0','U'),(611,'active binlog产生的周期(秒)','HeartbeatInterval','Main/BinLog','0','U'),(612,'按照OrderItem排序时，是否采用降序 Y/N','OrderDesc','Main/Cache','0','U'),(613,'数据迁移时传输数据进行压缩','transferCompress','Main/Cache','0','U');
/*!40000 ALTER TABLE `t_config_item` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_idc_map`
--

DROP TABLE IF EXISTS `t_idc_map`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_idc_map` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `city` varchar(255) NOT NULL COMMENT 'city name',
  `idc` varchar(32) NOT NULL COMMENT 'idc area',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4;

--
-- Table structure for table `t_transfer_status`
--

DROP TABLE IF EXISTS `t_transfer_status`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_transfer_status` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `app_name` varchar(50) NOT NULL DEFAULT '',
  `module_name` varchar(50) NOT NULL DEFAULT '',
  `src_group` varchar(128) NOT NULL DEFAULT '',
  `dst_group` varchar(120) NOT NULL DEFAULT '',
  `status` tinyint(4) NOT NULL DEFAULT '0',
  `router_transfer_id` text,
  `auto_updatetime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `transfer_start_time` varchar(32) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `transfer_record` (`app_name`,`module_name`,`src_group`,`dst_group`)
) ENGINE=InnoDB AUTO_INCREMENT=52975 DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

--
-- Table structure for table `t_expand_status`
--

DROP TABLE IF EXISTS `t_expand_status`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_expand_status` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `app_name` varchar(50) NOT NULL,
  `module_name` varchar(50) NOT NULL,
  `status` tinyint(4) NOT NULL,
  `router_transfer_id` text,
  `auto_updatetime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `type` int(4) DEFAULT '0',
  `expand_start_time` varchar(32) DEFAULT NULL,
  `modify_group_name` text,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=1999 DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `t_router_switch`
--

DROP TABLE IF EXISTS `t_router_switch`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_router_switch` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `app_name` varchar(128) NOT NULL,
  `module_name` varchar(128) NOT NULL,
  `group_name` varchar(128) DEFAULT NULL,
  `master_server` varchar(128) DEFAULT NULL,
  `slave_server` varchar(128) DEFAULT NULL,
  `mirror_idc` varchar(128) DEFAULT NULL,
  `master_mirror` varchar(128) DEFAULT NULL,
  `slave_mirror` varchar(128) DEFAULT NULL,
  `switch_type` int(11) NOT NULL,
  `switch_result` int(11) NOT NULL DEFAULT '0',
  `access_status` int(11) NOT NULL DEFAULT '0',
  `comment` varchar(256) DEFAULT NULL,
  `switch_time` datetime DEFAULT NULL,
  `modify_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `switch_property` enum('auto','manual') DEFAULT 'manual',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=18083 DEFAULT CHARSET=utf8mb4;
/*!40101 SET character_set_client = @saved_cs_client */;


-- Dump completed on 2018-11-29 11:18:13



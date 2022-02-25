--1、业务模块表
CREATE TABLE `t_router_module` (
  `id` int(11) NOT NULL auto_increment,
  `module_name` varchar(255) NOT NULL,
  `version` int(11) NOT NULL,
  `remark` varchar(255) default NULL,
  `modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `POSTTIME` datetime default NULL,
  `LASTUSER` varchar(60) default NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY (`module_name`)
) ENGINE=MyISAM DEFAULT CHARSET=gbk;

-- module_name，业务模块名，全局唯一
-- version，模块版本号，修改后递增一
-- remark，描述信息


--2、服务器信息表
CREATE TABLE `t_router_server` (
  `id` int(11) NOT NULL auto_increment,
  `server_name` varchar(255) NOT NULL,
  `ip` varchar(15) NOT NULL,
  `binlog_port` int(11) NOT NULL,
  `cache_port` int(11) NOT NULL,
  `routerclient_port` int(11) NOT NULL,
  `idc_area`   varchar(10) NOT NULL DEFAULT 'sz',
  `remark` varchar(255) default NULL,
  `modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `POSTTIME` datetime default NULL,
  `LASTUSER` varchar(60) default NULL,
  PRIMARY KEY  (`id`),
  UNIQUE KEY (`server_name`)
) ENGINE=MyISAM DEFAULT CHARSET=gbk;

-- server_name，服务器名，全局唯一，为App.ServerName格式
-- ip，服务器IP
-- binlog_port，BinLog servant对象App.ServerName.BinLogObj的服务端口
-- cache_port，Cache servant对象App.ServerName.CacheObj的服务端口
-- routeclient_port，RouterClient servant对象App.ServerName.RouterClientObj的服务端口


--3、服务器组配置表
CREATE TABLE `t_router_group` (
  `id` int(11) NOT NULL auto_increment,
  `module_name` varchar(255) NOT NULL,
  `group_name` varchar(255) NOT NULL,
  `server_name` varchar(255) NOT NULL,
  `server_status` char(1) NOT NULL,
  `pri` tinyint NOT NULL DEFAULT 1,
  `source_server_name` varchar(255) default NULL,
  `modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `POSTTIME` datetime default NULL,
  `LASTUSER` varchar(60) default NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=gbk;

-- module_name，服务组所属的业务模块名
-- group_name，服务组名，全局唯一，为App.GroupName格式
-- server_name，服务名
-- server_status, 服务的状态，分为M/S
-- pri，优先级，越小优先级越高
-- source_server_name，备份的源服务名



--4、路由记录表
CREATE TABLE `t_router_record` (
  `id` int(11) NOT NULL auto_increment,
  `module_name` varchar(255) NOT NULL,
  `from_page_no` int(11) NOT NULL,
  `to_page_no` int(11) NOT NULL,
  `group_name` varchar(255) NOT NULL,
  `modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `POSTTIME` datetime default NULL,
  `LASTUSER` varchar(60) default NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=gbk;

-- module_name，路由所属业务模块名
-- from_page_no， 开始页
-- to_page_no，结束页
-- group_name，路由目的服务器组名


--5、迁移记录表
CREATE TABLE `t_router_transfer` (
  `id` int(11) NOT NULL auto_increment,
  `module_name` varchar(255) NOT NULL,
  `from_page_no` int(11) NOT NULL,
  `to_page_no` int(11) NOT NULL,
  `group_name` varchar(255) NOT NULL,
  `trans_group_name` varchar(255) NOT NULL,
  `transfered_page_no` int(11) default NULL,
  `remark` varchar(255) default NULL,
  `state` tinyint(4) NOT NULL default '0',
  `modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `POSTTIME` datetime default NULL,
  `LASTUSER` varchar(60) default NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=gbk;

-- module_name，迁移的业务模块名
-- from_page_no，迁移开始页号
-- to_page_no，迁移结束页号
-- group_name，迁移的源服务器组名
-- trans_group_name，迁移的目的服务器组名
-- transfered_page_no，已迁移的成功页
-- remark，迁移结果描述
-- state，当前迁移的状态，0|未迁移;1|正在迁移;2|迁移结束;4|停止迁移

--6 切换结果汇总表 at db_dcache_relation
CREATE TABLE `t_router_switch` (
  `id` int(11) NOT NULL auto_increment,
  `app_name` varchar(128) NOT NULL,
  `module_name` varchar(128) NOT NULL,
  `group_name` varchar(128),
  `master_server` varchar(128),
  `slave_server` varchar(128),
  `mirror_idc` varchar(128),
  `master_mirror` varchar(128),
  `slave_mirror` varchar(128),
  `switch_type` int(11) NOT NULL,
  `switch_result` int(11) NOT NULL default 0,
  `access_status` int(11) NOT NULL default 0,
  `comment` varchar(256),
  `switch_property` enum('auto','manual') default 'manual',
  `switch_time` datetime default NULL,
  `modify_time` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=gbk;


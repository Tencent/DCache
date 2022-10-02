## 背景

DCache是一个基于[TARS](https://github.com/TarsCloud/Tars)框架开发的分布式NoSQL存储系统，数据采用内存存储，支持连接后端DB实现数据持久化。DCache采用集群模式，具有高扩展、高可用的特点。

DCache在腾讯内部有大量业务使用，日访问总量超万亿次。

DCache具有以下特点

* 高性能存储引擎，支持key-value，k-k-row，list，set，zset多种数据结构，支持数据持久化落地后端DB。
* 集群模式，高扩展，高可用，支持异地镜像，就近接入。
* 通过名字访问，支持同步、异步、单向RPC调用。
* 高效运维平台，在线完成服务部署、扩缩容、迁移，以及服务配置，服务调用质量监控。
* 业务无须和直接和mysql交互, DCache会自动缓写DB

## 支持平台

> * Linux

## 开发语言

> * C++

## 快速上手

DCache的安装建议通过TarsWeb的服务市场安装.

## 文档

DCache接口使用方法请参考文档[Proxy接口指南](docs/proxy_api_guide.md)，更多文档请查看[docs目录](docs/README.md)

## 目录说明

* ```src```：存储相关的后台服务代码。

目录 |功能
------------------|----------------
src/Comm           |各服务共用的通用代码
src/ConfigServer   |DCache配置服务
src/DbAccess       |数据持久化DB的代理服务
src/KVCacheServer  |key-value存储服务
src/MKVCacheServer |k-k-row、list、set、zset存储服务
src/OptServer      |服务部署、运维管理，供web管理平台调用
src/PropertyServer |监控信息上报服务
src/Proxy          |DCache代理服务
src/Router         |DCache路由管理服务
src/TarsComm       |Tars数据结构定义

* ```docs```：文档。

## 数据库说明

DCache框架依赖mysql数据库, 当DCache安装以后, 它自动会创建以下数据库
- db_cache_web: DCache的web依赖
- db_dcache_relation: DCacheOptServer/ConfigServer/PropertyServer/DCacheWebServer依赖, 注意DCacheWebServer只是会创建它, 并不是直接使用它
- db_dcache_property: DCache统计数据库, PropertyServer依赖

需要在安装过程中正确设置配置文件中的数据库的地址.

实际每套创建的每套Cache, 如果设置了数据落地, 那么也依赖mysql, 这个在DCache管理平台上可以设置, 安装时正确配置即可.

## 参与贡献

如果你有任何想法，别犹豫，立即提 Issues 或 Pull Requests，欢迎所有人参与到DCache的开源建设中。<br>详见：[CONTRIBUTING.md](./CONTRIBUTING.md)


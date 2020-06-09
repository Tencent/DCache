DCache是一个基于[TARS](https://github.com/TarsCloud/Tars)框架开发的分布式NoSQL存储系统，数据采用内存存储，支持连接后端DB实现数据持久化。DCache采用集群模式，具有高扩展、高可用的特点。

DCache在腾讯内部有大量业务使用，日访问总量超万亿次。

DCache具有以下特点

* 高性能存储引擎，支持key-value，k-k-row，list，set，zset多种数据结构，支持数据持久化落地后端DB。
* 集群模式，高扩展，高可用，支持异地镜像，就近接入。
* 通过名字访问，支持同步、异步、单向RPC调用。
* 高效运维平台，轻松完成服务部署、扩缩容、迁移，以及服务配置，服务调用质量监控。


## 支持平台

> * Linux

## 支持语言

> * C++

## 快速上手

详见[DCache安装文档](docs/install.md)

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
src/thirdParty     |第三方依赖

* ```docs```：文档。

## 参与贡献

如果你有任何想法，别犹豫，立即提 Issues 或 Pull Requests，欢迎所有人参与到DCache的开源建设中。<br>详见：[CONTRIBUTING.md](./CONTRIBUTING.md)


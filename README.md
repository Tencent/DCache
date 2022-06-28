## 介绍

DCache是一个基于[TARS](https://github.com/TarsCloud/Tars)框架开发的分布式NoSQL存储系统，数据采用内存存储，支持连接后端DB实现数据持久化。DCache采用集群模式，具有高扩展、高可用的特点。

DCache在腾讯内部有大量业务使用，日访问总量超万亿次。

DCache具有以下特点

* 高性能存储引擎，支持key-value，k-k-row，list，set，zset多种数据结构，支持数据持久化落地后端DB。
* 集群模式，高扩展，高可用，支持异地镜像，就近接入。
* 通过名字访问，支持同步、异步、单向RPC调用。
* 高效运维平台，在线完成服务部署、扩缩容、迁移，以及服务配置，服务调用质量监控。
* 业务无须和直接和mysql交互, DCache会自动缓写DB

**目前DCache不支持K8SFramework**

## 组成说明

DCache由一系列服务组成, 通常分两类:
- 一类是管理类服务, 它在部署DCache系统时就需要存在, 主要用于管理Cache服务
>- DCache.DCacheOptServer: Cache的操作服务, 用来部署Cache服务
>- DCache.PropertyServer: Cache特殊的上报监控特性的服务, 注意Cache类服务的模板中, property指向到这个服务
>- DCache.ConfigServer: 提供生成Cache等服务需要的配置信息

- 一类是缓存服务, 它在每次创建一套Cache系统时动态部署在Tars平台上
>- RouterServer: 路由服务, 管理Cache服务的路由信息
>- ProxyServer: Cache服务的请求入口代理
>- KVCacheServer: key-value模式的cache服务
>- MKVCacheServer: k-k-row，list，set，zset等模式的cache服务
>- CombineDbAccessServer: 访问mysql数据库的代理服务

## 支持说明

在< TarsWeb:v3.0.3 之前, DCache管理平台被内置在 TarsWeb 中, 之后版本为了提高 TarsWeb 的扩展性, TarsWeb 支持了服务扩展化, 即你可以实现独立的 web 服务和 TarsWeb 整合到一起, 从而当子模块服务升级时无须升级 TarsWeb, 具体方式[请参考 TarsWeb 相关的文档](https://doc.tarsyun.com/#/base/plugins.md).

## 安装方式

推荐使用新版本 >= tarscloud/framework:v3.0.10 时, 直接从云市场安装系统, 建议以容器方式启动, 这样不依赖操作系统 stdc++.so 的版本.

[容器方式启动业务方式请参考](https://doc.tarsyun.com/#/installation/service-docker.md)

## mysql 配置说明

在安装系统时, 需要依赖 mysql, 因此在安装注意配置依赖的 mysql 地址

- DCacheOptServer 请修改`DCacheOptServer.conf`
- PropertyServer 请修改`PropertyServer.conf`
- ConfigServer 请修改`ConfigServer.conf`

数据库请使用同一个, 注意安装时, 这些配置中的数据库信息需要配置正确!

## 其他说明

具体说明请升级框架以后, 进入TarsWeb的服务市场, 在产品中选择dcache-system系统查看和安装.


如果您希望源码安装, 请详见[DCache安装文档](docs/install.md), 对于Tars新版本, 源码安装没有自动发布web管理平台, 需要您手工发布.

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

## 参与贡献

如果你有任何想法，别犹豫，立即提 Issues 或 Pull Requests，欢迎所有人参与到DCache的开源建设中。<br>详见：[CONTRIBUTING.md](./CONTRIBUTING.md)


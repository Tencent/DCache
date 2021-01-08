# 安装DCache

> * [依赖环境](#1)
> * [准备工作](#2)
> * [服务部署](#3)
> * [创建一个DCache应用](#4)
>   * [部署和发布Proxy和Router服务](#4.1)
>   * [上线一个KVCache模块](#4.2)
>   * [上线一个MKVCache模块](#4.3)
>   * [Cache配置管理](#4.4)


## <a id = "1"></a> 1. 依赖环境

DCache(>=2.1.1)是基于[Tars](https://github.com/TarsCloud/Tars)框架（版本>=v2.4.6）开发，所以编译之前请先安装Tars开发环境和管理平台，安装步骤请参考[Tars的install文档](https://github.com/TarsCloud/Tars/blob/master/Install.zh.md)。安装完Tars管理平台后，在浏览器中访问管理平台主页，如下图：

![Tars管理平台主页](images/tars_mainPage.png)

## <a id = "2"></a> 2. 准备工作

### 2.1 编译

在源码目录执行：`mkdir build; cd build; cmake ..; make; make release; make tar`

即可生成各服务的发布包。

### 2.2 创建模板

在Tars的Web平台创建DCache.Cache模板，后续部署DCache模块时会用到该模板, 如果已经存在, 则忽略这步骤

![创建模板](images/tars_add_tmplate.png)

新增模板DCache.Cache，父模板选择tars.default，模板内容填入：

```xml
<tars>
    <application>
        <client>
            property=DCache.PropertyServer.PropertyObj
        </client>
    </application>
</tars>
```

## <a id = "3"></a> 3. 服务部署

DCache(>=2.1.1) 支持脚本一站式部署服务!

进入build目录(必须在build下执行)
```
cd build
../deploy/install.sh TARS_MYSQL_IP TARS_MYSQL_PORT TARS_MYSQL_USER TARS_MYSQL_PASSWORD DCACHE_MYSQL_IP DCACHE_MYSQL_PORT DCACHE_MYSQL_USER DCACHE_MYSQL_PASSWORD CREATE WEB_HOST WEB_TOKEN NODE_IP
```

注意:
- TARS_MYSQL_IP: tars数据库的ip
- TARS_MYSQL_PORT: tars数据库的port
- TARS_MYSQL_USER: tars数据库的user
- TARS_MYSQL_PASSWORD: tars数据库的密码
- DCACHE_MYSQL_IP: dcache数据库的ip
- DCACHE_MYSQL_PORT: dcache数据库的port
- DCACHE_MYSQL_USER: dcache数据库的user
- DCACHE_MYSQL_PASSWORD: dcache数据库的密码
- CREATE: 是否重新建表，`true` 表示重新创建 DCache 表，`false` 表示不创建
- WEB_HOST: TARS web平台地址
- WEB_TOKEN: TARS web平台token(需要进入web平台, 用户中心上, 创建一个Token)
- NODE_IP: 公共服务部署节点IP, 部署完成后, 你可以在web平台扩容到多台节点机上

执行install.sh脚本后, DCache所有基础环境就准备完毕了:
- Router、Cache、Proxy等发布包已经上传
- DCacheOptServer/PropertyServer/ConfigServer的配置和服务包已经上传

下一步就可以创建DCache的模块了.

## <a id = "4"></a> 4. 创建DCache应用

> 本节描述如何创建一个DCache应用，如何上传发布包，如何上线一个模块，并对模块进行配置。
>
> 名词解释
>
> * 模块：类似于mysql中table的概念，使用者创建一个模块来存储数据。模块分为KVCache和MKVCache两种，如果要存储key-value数据则创建KVCache模块，如果要使用k-k-row，list，set，zset则创建MKVCache模块。
> * 应用：应用是多个模块的集合，应用下所有模块共享Proxy和Router服务，类似于mysql中db的概念。

### <a id = "4.1"></a> 4.1 部署和发布Proxy和Router服务

![安装DCache应用](images/install_dcache_app.png)

根据上图，依次点击“服务创建”，自定义“应用”名称，然后点击“创建应用”，得到下图：

![创建proxy和router服务](images/create_proxy&router.png)

在输入框填写相关信息，其余保持不变，点击“创建router、proxy服务”，得到下图：

![确认信息](images/install_and_release.png)

确认填写无误后，点击“安装发布”，等待安装完成，结果如下图所示：

![成功安装proxy和router](images/install_proxy&router_succ.png)

**注意DBAccess服务部署也在界面中完成**

### <a id = "4.2"></a> 4.2 上线一个KVCache模块

![创建KVCache模块](images/create_KV_module.png)

按照上图箭头依次点击，“应用”选择在[部署和发布Proxy和Router服务](#5.2)创建的应用名称，“cache类型”选择KVCache，所填信息确认无误后，点击“下一步”进入“模块配置”步骤，如下图：

![模块配置](images/KV_module_conf.png)

填写必要信息之后，点击“下一步”，进入“服务配置”步骤，如下图：

![服务配置](images/KV_service_conf.png)

**注意：** <font color=red>共享内存key必须是唯一的，不能在服务部署机器上已存在，否则会造成服务拉起失败，可使用**ipcs**命令确认。</font>  

Cache服务的模板默认会选择DCache.Cache，如果模板DCache.Cache不存在，可创建该模板或者选择其他可用的模板，模板中必须配置property，这样才能查看服务的特性监控数据。

```xml
<tars>
    <application>
        <client>
            property=DCache.PropertyServer.PropertyObj
        </client>
    </application>
</tars>
```

必要信息填写完毕，点击“下一步”进入“安装发布”步骤，等待服务发布完成。刷新管理平台主页，左侧目录树出现此模块信息，如下图：

![安装成功](images/install_kv_succ.png)

### <a id = "4.3"></a> 4.3 上线一个MKVCache模块

步骤和[部署和发布KVCache](#5.3)类似，参考即可。

### <a id = "4.4"></a> 4.4 Cache配置管理

![Cache配置管理](images/cache_config.png)

按照上图箭头依次点击，可添加配置项。

#### 模块和单节点的配置管理

![模块配置管理](images/add_conf_for_module.png)

按照上图箭头依次点击，可在该页面上修改和添加配置。该页面的配置管理分两种类型：针对模块所有节点的配置管理和针对模块特定节点的配置管理。如果节点配置和模块配置有重叠的配置项，那么节点配置将覆盖模块配置。
**注意：** <font color=red>修改配置后，需要重启服务才能生效。</font>  

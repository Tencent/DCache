## v2.2.1 2022.04.02

### en

- Add: support cloud market

### en

- Add: 支持云市场

## v2.2.0 2021.03.20

### en

- Add: add kv use demo in c++
- Fix: Install command miss a param CREATE
- Add: link_libraries(pthread)
- Fix: fix some equal bug
- Fix: 修改 cmakefile，解决 make 失败的问题
- Fix: TLOG_DEBUG & TLOG_ERROR
- Fix: shmget, use ShmKey.dat when ShmKey exists
- Add: report error message when cache start
- Fix: create table routeclient_port -> routerclient_port

## v2.1.1 2020.08.05

### en

- update deploy, auto deploy server config
- DCacheOptServer add get tree interface
- Finish DbAccessServer create and deploy

### en

- 更新自动部署逻辑
- DCacheOptServer 添加获取 cache 目录树接口
- 完成 DbAccess 服务的创建和部署

## v2.1.0 2020.07.20

### en

- support tars-web v2.4.7
- add CombinDbAccessServer
- tars-web(>=v2.4.7) support auto deploy router, proxy, cache, dbaccess

### en

- 支持 tars-web v2.4.7
- 添加 CombinDbAccessServer
- tars-web(>=v2.4.7) 支持自动部署各个模块

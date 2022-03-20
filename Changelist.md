

# v2.2.0 2021.03.20
- Add: add kv use demo in c++
- Fix: Install command miss a param CREATE
- Add: 增加  link_libraries(pthread)
- Fix: fix some equal bug
- Fix: 修改cmakefile，解决make失败的问题
- Fix: TLOG_DEBUG & TLOG_ERROR
- Fix: shmget, use ShmKey.dat when ShmKey exists
- Add: report error message when cache start
- Fix:  create table routeclient_port -> routerclient_port

# v2.1.1 2020.08.05
- update deploy, auto deploy server config
- DCacheOptServer add get tree interface
- Complete the creation and deployment of dbaccess service

# v2.1.0 2020.07.20
- support tars-web v2.4.7
- add CombinDbAccessServer
- tars-web(>=v2.4.7) support auto deploy router, proxy, cache, dbaccess
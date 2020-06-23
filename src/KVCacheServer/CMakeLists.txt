
gen_server(DCache KVCacheServer)

include_directories(../Comm)
include_directories(../Router)
include_directories(../DbAccess)
include_directories(../ConfigServer)
add_dependencies(KVCacheServer cache_comm RouterServer TarsComm)

target_link_libraries(KVCacheServer mysqlclient cache_comm)

gen_dcache_group(KVCacheServer)

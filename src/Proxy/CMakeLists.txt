
gen_server(DCache ProxyServer)

include_directories(../Router)
include_directories(../KVCacheServer)
include_directories(../MKVCacheServer)

add_dependencies(ProxyServer RouterServer MKVCacheServer KVCacheServer)

target_link_libraries(ProxyServer cache_comm)
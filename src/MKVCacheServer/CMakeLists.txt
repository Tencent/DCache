
include_directories(../Router)

gen_server(DCache MKVCacheServer)

add_dependencies(MKVCacheServer RouterServer)

target_link_libraries(MKVCacheServer cache_comm z)

gen_dcache_group(MKVCacheServer)

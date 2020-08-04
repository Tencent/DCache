
include_directories(../Router)

gen_server(DCache MKVCacheServer)

gen_dcache_group(MKVCacheServer)

add_dependencies(MKVCacheServer RouterServer)

target_link_libraries(MKVCacheServer cache_comm z)


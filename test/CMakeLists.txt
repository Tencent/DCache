

include_directories(../src/Comm)
include_directories(../src/TarsComm)
include_directories(../src/Proxy)
include_directories(../src/Router)
include_directories(../src/KVCacheServer)
include_directories(../src/MKVCacheServer)

add_subdirectory(Proxy)
add_subdirectory(Router)
add_subdirectory(KVCacheServer)

#add_dependencies(test-ProxyServer cache_common TarsComm ProxyServer RouterServer KVCacheServer MKVCacheServer)
#add_dependencies(test-RouterServer cache_common TarsComm ProxyServer RouterServer KVCacheServer MKVCacheServer)
#add_dependencies(test-KVCacheServer cache_common TarsComm ProxyServer RouterServer KVCacheServer MKVCacheServer)

link_libraries(mysqlclient gtest cache_common)

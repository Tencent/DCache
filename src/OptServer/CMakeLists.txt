
include_directories(/usr/local/tars/cpp/include/framework)
include_directories(../Proxy)
include_directories(../Router)
include_directories(../PropertyServer)

gen_server(DCache OptServer)

add_dependencies(OptServer PropertyServer ProxyServer RouterServer)

target_link_libraries(OptServer mysqlclient)
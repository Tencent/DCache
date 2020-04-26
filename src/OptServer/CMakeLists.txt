
include_directories(/usr/local/tars/cpp/include/framework)
include_directories(/usr/local/tars/cpp/include/servant)
include_directories(../Proxy)
include_directories(../Router)
include_directories(../PropertyServer)

gen_server(DCache DCacheOptServer)

add_dependencies(DCacheOptServer PropertyServer ProxyServer RouterServer)

target_link_libraries(DCacheOptServer mysqlclient)
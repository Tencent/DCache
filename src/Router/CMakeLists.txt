
include_directories(/usr/local/tars/cpp/include/framework)

gen_server(DCache RouterServer)

add_dependencies(RouterServer TarsComm)

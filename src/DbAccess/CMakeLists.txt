
gen_server(DCache DbAccessServer)

add_dependencies(DbAccessServer TarsComm)

target_link_libraries(DbAccessServer mysqlclient)

include_directories(${CMAKE_SOURCE_DIR}/tarscpp/util/include)

gen_server(DCache mysql-tool)

target_link_libraries(mysql-tool ${LIB_MYSQL})

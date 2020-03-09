cmake_minimum_required(VERSION 2.8)

project(DCache)

option(TARS_MYSQL "option for mysql" ON)
option(TARS_SSL "option for ssl" OFF)
option(TARS_HTTP2 "option for http2" OFF)

if(WIN32)
    include (c:\\tars\\cpp\\makefile\\tars-tools.cmake)
else()
    include (/usr/local/tars/cpp/makefile/tars-tools.cmake)
endif()

set(TARS_TOOL_FLAG "--with-tars")
set(TARS_WEB_HOST "http://127.0.0.1:4001")

# include_directories(/usr/local/taf/cpp/thirdparty/include)
# link_directories(/usr/local/taf/cpp/thirdparty/lib)

#include_directories(/home/tarsprotol/App/OtherServer)

add_subdirectory(src)
# add_subdirectory(test)
link_libraries(mysqlclient)


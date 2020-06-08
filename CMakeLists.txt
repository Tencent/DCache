cmake_minimum_required(VERSION 2.8)

project(DCache)

option(TARS_MYSQL "option for mysql" ON)
option(TARS_SSL "option for ssl" OFF)
option(TARS_HTTP2 "option for http2" OFF)

set(TARS_TOOL_FLAG "--with-tars")
set(TARS_WEB_HOST "http://127.0.0.1:4001")

if(WIN32)
    include (c:\\tars\\cpp\\makefile\\tars-tools.cmake)
else()
    include (/usr/local/tars/cpp/makefile/tars-tools.cmake)
endif()

include_directories(/usr/local/tars/cpp/thirdparty/include)
link_directories(/usr/local/tars/cpp/thirdparty/lib)

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(mysql-tool)


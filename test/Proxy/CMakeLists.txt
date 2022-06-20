
aux_source_directory(../../src/Proxy DIR_SRC)

list(REMOVE_ITEM DIR_SRC "../../src/Proxy/main.cpp")

add_library(libProxy ${DIR_SRC})

file(GLOB_RECURSE TEST_CPPS *.cpp)

link_directories(/usr/local/tars/cpp/thirdparty/lib64)

foreach(TEST_CPP ${TEST_CPPS})
    get_filename_component(TEST_NAME ${TEST_CPP} NAME_WE)

    add_executable(test-${TEST_NAME} ${TEST_CPP})

    target_link_libraries(test-${TEST_NAME} mysqlclient gtest gmock libProxy cache_comm tarsservant tarsutil ssl crypto nghttp2)

    add_dependencies(test-${TEST_NAME} libProxy cache_comm TarsComm ProxyServer RouterServer KVCacheServer MKVCacheServer)

endforeach()




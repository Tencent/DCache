

aux_source_directory(../../src/KVCacheServer DIR_SRC)
aux_source_directory(../../src/KVCacheServer/jmem_hashmap_malloc CACHE_DIR_SRC)

list(REMOVE_ITEM DIR_SRC "../../src/KVCacheServer/main.cpp")

add_library(libKVCacheServer ${DIR_SRC} ${CACHE_DIR_SRC})

file(GLOB_RECURSE TEST_CPPS *.cpp)

foreach(TEST_CPP ${TEST_CPPS})
    get_filename_component(TEST_NAME ${TEST_CPP} NAME_WE)

    add_executable(test-${TEST_NAME} ${TEST_CPP})

    target_link_libraries(test-${TEST_NAME} mysqlclient gtest gmock tarsservant tarsutil cache_comm libKVCacheServer)

    add_dependencies(test-${TEST_NAME} libKVCacheServer cache_comm TarsComm ProxyServer RouterServer KVCacheServer MKVCacheServer)

endforeach()



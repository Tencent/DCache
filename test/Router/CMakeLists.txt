
aux_source_directory(../../src/Router DIR_SRC)

list(REMOVE_ITEM DIR_SRC "../../src/Router/main.cpp")

add_library(libRouter ${DIR_SRC})

file(GLOB_RECURSE TEST_CPPS *.cpp)

foreach(TEST_CPP ${TEST_CPPS})
    get_filename_component(TEST_NAME ${TEST_CPP} NAME_WE)

    add_executable(test-${TEST_NAME} ${TEST_CPP})

    target_link_libraries(test-${TEST_NAME} mysqlclient gtest gmock cache_comm libRouter tarsservant tarsutil)

    add_dependencies(test-${TEST_NAME} libRouter cache_comm TarsComm ProxyServer RouterServer KVCacheServer MKVCacheServer)

endforeach()





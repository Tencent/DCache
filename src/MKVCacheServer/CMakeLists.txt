
include_directories(../Router)

gen_server(DCache MKVCacheServer)

gen_dcache_group(MKVCacheServer)

add_dependencies(MKVCacheServer RouterServer)

target_link_libraries(MKVCacheServer cache_comm z)

set(TARGET "MKVCacheServer")
if(WIN32)
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.h c:/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.h c:/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.tars c:/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.tars c:/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.h c:/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.h c:/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.tars c:/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.tars c:/tarsproto/protocol/${APP}/${TARGET})\n")
elseif(APPLE)
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.h $ENV{HOME}/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.h $ENV{HOME}/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.tars $ENV{HOME}/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.tars $ENV{HOME}/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.h $ENV{HOME}/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.h $ENV{HOME}/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.tars $ENV{HOME}/tarsproto/protocol/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.tars $ENV{HOME}/tarsproto/protocol/${APP}/${TARGET})\n")
elseif(UNIX)
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.h /home/tarsproto/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.h /home/tarsproto/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.tars /home/tarsproto/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/ProxyShare.tars /home/tarsproto/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.h /home/tarsproto/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.h /home/tarsproto/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E echo cp -rf ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.tars /home/tarsproto/${APP}/${TARGET})\n")
    FILE(APPEND ${RUN_RELEASE_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/src/TarsComm/CacheShare.tars /home/tarsproto/${APP}/${TARGET})\n")
endif()
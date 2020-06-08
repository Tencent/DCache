
include_directories(Comm)
include_directories(TarsComm)

macro(gen_dcache_group TARGET)
    SET(RUN_TAR_COMMAND_FILE "${CMAKE_BINARY_DIR}/run-tar-DG-${TARGET}.cmake")
    FILE(WRITE ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/tmp/DCacheServerGroup)\n")
    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tmp/DCacheServerGroup)\n")
    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/bin/${TARGET} ${CMAKE_BINARY_DIR}/tmp/DCacheServerGroup)\n")
    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/tmp/ tar czfv DG-${TARGET}.tgz DCacheServerGroup)\n")
    FILE(APPEND ${RUN_TAR_COMMAND_FILE} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/tmp/DG-${TARGET}.tgz ${CMAKE_BINARY_DIR}/DG-${TARGET}.tgz)\n")

    add_custom_command(OUTPUT DG-${TARGET}.tgz
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} -P ${RUN_TAR_COMMAND_FILE}
            COMMENT "call ${RUN_TAR_COMMAND_FILE}")

    FILE(APPEND ${TARS_TAR} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -P ${RUN_TAR_COMMAND_FILE})\n")
    FILE(APPEND ${TARS_TAR} "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${TARGET}.tgz)\n")
endmacro()

add_subdirectory(Comm)
add_subdirectory(ConfigServer)
add_subdirectory(DbAccess)
add_subdirectory(KVCacheServer)
add_subdirectory(MKVCacheServer)
add_subdirectory(OptServer)
add_subdirectory(PropertyServer)
add_subdirectory(Proxy)
add_subdirectory(Router)
add_subdirectory(TarsComm)
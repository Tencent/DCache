
message("in src ${DBACCESS_SERVER}..." )

#include_directories("../MKDbAccess")

gen_server(DCache ${DBACCESS_SERVER})

add_dependencies(${DBACCESS_SERVER} ${COMM})

target_link_libraries(${DBACCESS_SERVER} mysqlclient)

add_custom_target(tar_dbacc
        COMMAND sh ${PROJECT_SOURCE_DIR}/script/tar.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DCache ${DBACCESS_SERVER} )
add_dependencies(tar_dbacc ${DBACCESS_SERVER})
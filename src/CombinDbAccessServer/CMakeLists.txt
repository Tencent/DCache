
message("in src CombinDbAccessServer..." )

#include_directories("../MKDbAccess")

gen_server(DCache CombinDbAccessServer)

add_dependencies(CombinDbAccessServer cache_comm)

target_link_libraries(CombinDbAccessServer mysqlclient)

#add_custom_target(tar_dbacc COMMAND sh ${PROJECT_SOURCE_DIR}/script/tar.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} DCache ${DBACCESS_SERVER} )
#add_dependencies(tar_dbacc ${DBACCESS_SERVER})
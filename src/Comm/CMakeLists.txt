
aux_source_directory(. DIR_SRC)

include_directories(../Router)

add_library(cache_comm ${DIR_SRC})

add_dependencies(cache_comm RouterServer)


aux_source_directory(. DIR_SRC)

add_library(cache_comm ${DIR_SRC})

add_dependencies(cache_comm RouterServer)
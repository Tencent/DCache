include ../make.app
#-----------------------------------------------------------------------
TARGET    := KVCacheServer
CONFIG    :=
DFLAGS    := O2
LOCAL_SRC  += $(wildcard ./jmem_hashmap_malloc/*.cpp)
INCLUDE    += -I../Comm -I../TarsComm/ -I./jmem_hashmap_malloc/
LIB        += -L../lib -lcache_comm -L/usr/local/lib -lz

APP_DIR := /home/tarsproto/$(APP)
#-----------------------------------------------------------------------
include $(APP_DIR)/ConfigServer/ConfigServer.mk
include $(APP_DIR)/DbAccessServer/DbAccessServer.mk
include $(APP_DIR)/RouterServer/RouterServer.mk
include /usr/local/tars/cpp/makefile/makefile.tars
#-----------------------------------------------------------------------
tar:
	mkdir -p ./DCacheServerGroup
	cp ./KVCacheServer ./DCacheServerGroup/KVCacheServer
	tar czvf ./DCacheServerGroup.tgz ./DCacheServerGroup
	rm -rf ./DCacheServerGroup


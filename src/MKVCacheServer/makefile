include ../make.app
#-----------------------------------------------------------------------
TARGET    := MKVCacheServer
CONFIG    :=
DFLAGS := O2
LOCAL_SRC  += $(wildcard ./jmem_multi_hashmap_malloc/*.cpp)
INCLUDE    += -I../TarsComm/ -I../Comm -I./jmem_multi_hashmap_malloc
LIB        += -L../lib -lcache_comm -L/usr/local/lib -lz

APP_DIR := /home/tarsproto/$(APP)
#-----------------------------------------------------------------------
include $(APP_DIR)/ConfigServer/ConfigServer.mk
include $(APP_DIR)/RouterServer/RouterServer.mk
include $(APP_DIR)/DbAccessServer/DbAccessServer.mk
include /usr/local/tars/cpp/makefile/makefile.tars
#-----------------------------------------------------------------------
tar:
	mkdir -p ./DCacheServerGroup
	cp ./MKVCacheServer ./DCacheServerGroup/MKVCacheServer
	tar czvf ./DCacheServerGroup.tgz ./DCacheServerGroup
	rm -rf ./DCacheServerGroup

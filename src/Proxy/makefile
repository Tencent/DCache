include ../make.app
#-----------------------------------------------------------------------
TARGET    := ProxyServer
INCLUDE   += -I../Comm -I../TarsComm
LIB       += -L../lib -lcache_comm
DFLAGS    := O2
APP_DIR   := /home/tarsproto/$(APP)

#-----------------------------------------------------------------------
include $(APP_DIR)/RouterServer/RouterServer.mk
include $(APP_DIR)/KVCacheServer/KVCacheServer.mk
include $(APP_DIR)/MKVCacheServer/MKVCacheServer.mk
include /usr/local/tars/cpp/makefile/makefile.tars
#-----------------------------------------------------------------------


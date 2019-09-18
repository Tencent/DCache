include ../make.app
#-----------------------------------------------------------------------
TARGET    := DCacheOptServer
MFLAGS    :=
CONFIG    :=
STRIP_FLAG:= N
MFLAGS    := 64
TARS2CPP_FLAG := --with-tars

INCLUDE   += -I/usr/include/mysql
LIB       += -L/usr/lib64/mysql/ -lmysqlclient 

APP_DIR := /home/tarsproto/$(APP)
#----------------------------------------------------------------------
include  $(APP_DIR)/ProxyServer/ProxyServer.mk
include  $(APP_DIR)/RouterServer/RouterServer.mk
include  $(APP_DIR)/PropertyServer/PropertyServer.mk
#-----------------------------------------------------------------------

include /usr/local/tars/cpp/makefile/makefile.tars

#-----------------------------------------------------------------------

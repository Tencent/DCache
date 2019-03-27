include ../make.app
#-----------------------------------------------------------------------
TARGET     := RouterServer
CONFIG     :=
CFLAGS     :=
INCLUDE    += -I/usr/local/mysql/include/mysql -I../TarsComm -I../thirdParty/rapidjson/include
LIB        += -L/usr/lib64/mysql/ -ltarsutil -lmysqlclient

APP_DIR := /home/tarsproto/$(APP)
#-----------------------------------------------------------------------
include /usr/local/tars/cpp/makefile/makefile.tars


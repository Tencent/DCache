include ../make.app
#-----------------------------------------------------------------------
TARGET    := DbAccessServer
MFLAGS    :=
DFLAGS    := O2
CONFIG    := 
STRIP_FLAG:= N

INCLUDE   += -I../TarsComm
LIB       += -L/usr/lib64/mysql/ -lmysqlclient

APP_DIR := /home/tarsproto/$(APP)
#-----------------------------------------------------------------------
include /usr/local/tars/cpp/makefile/makefile.tars
#-----------------------------------------------------------------------


include ../make.app
#-----------------------------------------------------------------------

APP       := DCache
TARGET    := CombinDbAccessServer
MFLAGS    :=
CONFIG    := 
STRIP_FLAG:= N
J2CPP_FLAG:= 

#INCLUDE   += -I/usr/local/mqq/wbl/include -I/home/carelye/mysql-5.1.59-linux-x86_64-glibc23/include/ -I/home/tafjce/DCache/CacheShare
#LIB       += -L/usr/local/mqq/wbl/lib -lwbl -L/home/carelye/mysql-5.1.59-linux-x86_64-glibc23/lib/ -lmysqlclient

INCLUDE   += -I../Comm -I/usr/local/mysql/include -I/home/tafjce/DCache/CacheShare
LIB       += -L/usr/local/mysql/lib/mysql/ -L../lib -lcache_comm -lmysqlclient -lz
APP_DIR := /home/tafjce/$(APP)

#-----------------------------------------------------------------------

include /usr/local/taf/cpp/makefile/makefile.taf.new

#-----------------------------------------------------------------------

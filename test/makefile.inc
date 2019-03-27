TARS_PATH    :=/usr/local/tars/cpp

CC          = gcc
CXX         = g++
CFLAGS      = -std=c++11 -g -O2 -Wno-deprecated -Wall -Wno-strict-aliasing -Wno-sign-compare

MYSQL_INC       += -I/usr/local/mysql/include/mysql -I/usr/local/mysql/include -I/usr/include/mysql
MYSQL_LIB_DIR   += -L/usr/local/mysql/lib/mysql -L/usr/local/mysql/lib -L/usr/lib/mysql -L/usr/lib64/mysql/
LIB_DIR         += ${MYSQL_LIB_DIR}

GTEST_INC       += -I/data/guochengwu/gtest-src/googletest-release-1.8.1/googletest/include -I/data/guochengwu/gtest-src/googletest-release-1.8.1/googlemock/include
GTEST_LIB_DIR   += -L/data/guochengwu/gtest-src/googletest-release-1.8.1
LIB_DIR         += ${GTEST_LIB_DIR}

INCLUDE         += -I${TARS_PATH}/include ${MYSQL_INC} ${GTEST_INC} -I../../src/thirdParty/rapidjson/include
LIB             += -L${TARS_PATH}/lib ${LIB_DIR} -ltarsservant -ltarsparse -ltarsutil -lpthread -lz -lgtest -lmysqlclient -L/usr/lib64/mysql/

TEST_SRCS = $(wildcard *.cpp)
TEST_BINS = $(TEST_SRCS:.cpp=)
TEST_OBJS = $(addsuffix .o, $(basename $(TEST_SRCS)))

MODULE_NAME = $(notdir $(shell pwd))
DIRS := $(shell find ../../src/$(MODULE_NAME) -maxdepth 3 -type d)
MODULE_OBJS := $(foreach dir,$(DIRS),$(wildcard $(dir)/*.o))

MODULE_LIB = lib$(shell echo $(MODULE_NAME) | tr A-Z a-z ).dbg.a

.PHONY:all
all:$(MODULE_LIB) $(TEST_BINS)

.PHONY:clean
clean:
	@echo "rm -rf $(TEST_BINS) $(TEST_OBJS)"
	@rm -rf $(TEST_BINS) $(TEST_OBJS) $(MODULE_LIB)

$(TEST_OBJS): %.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDE) -o $@ -c $<

$(TEST_BINS): % : %.o
	$(CXX) $(CFLAGS) -o $@ $< $(MODULE_LIB) $(INCLUDE) $(LIB)

$(MODULE_LIB):$(filter-out $(wildcard *main.o), $(MODULE_OBJS))
	@echo "packing $@ : $^"
	@ar r $@ $^

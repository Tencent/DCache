include ./make.app
export -e APP
TOPDIR	:= .

DIRS            = TarsComm ConfigServer Router Comm KVCacheServer MKVCacheServer Proxy PropertyServer OptServer
DIRS_RElEASE    = TarsComm ConfigServer DbAccess Router KVCacheServer MKVCacheServer Proxy OptServer

all:
	@for dir in $(DIRS); do make -C $$dir; echo; done

clean:
	@for dir in $(DIRS); do make -C $$dir clean; echo; done

cleanall:
	@for dir in $(DIRS); do make -C $$dir cleanall; echo; done

release:
	mkdir -p lib
	@for dir in $(DIRS_RElEASE); do make -C $$dir release; echo; done

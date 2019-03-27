include ../make.app

TARS2CPP=/usr/local/tars/cpp/tools/tars2cpp

all:
	$(TARS2CPP) ProxyShare.tars CacheShare.tars

release:
	$(TARS2CPP) ProxyShare.tars CacheShare.tars
	mkdir -p /home/tarsproto/$(APP)/ProxyServer
	cp ProxyShare.tars ProxyShare.h CacheShare.tars CacheShare.h /home/tarsproto/$(APP)/ProxyServer
	mkdir -p /home/tarsproto/$(APP)/KVCacheServer
	cp ProxyShare.tars ProxyShare.h CacheShare.tars CacheShare.h /home/tarsproto/$(APP)/KVCacheServer
	mkdir -p /home/tarsproto/$(APP)/MKVCacheServer
	cp ProxyShare.tars ProxyShare.h CacheShare.tars CacheShare.h /home/tarsproto/$(APP)/MKVCacheServer
clean:
	rm ProxyShare.h CacheShare.h

cleanall:
	rm ProxyShare.h CacheShare.h

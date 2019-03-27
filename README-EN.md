[点我查看中文版](README.md)

DCache is a distributed NoSQL system based on  [Tars framework](https://github.com/TarsCloud/Tars), which supports LRU algorithm and data persistence.

There are many teams int Tencent that use DCache, and the total number of daily access exceeds one trillion.

## Features Of DCache

* Support a variety of data structures: including key-value, k-k-row, list, set, zset.
* Distributed storage with good scalability.
* An LRU cache, support expiration mechanism and data persistence.
* With a perfect platform for maintaining and monitoring. 

## Supported operating systems

> * Linux

## Supported programming languages

> * C++

## Documents

For API usage, see [proxy_api_guide-en.md](docs-en/proxy_api_guide-en.md),for more details, see [docs](docs-en/).

## Installation

See [install-en.md](docs/install.md)

## Overview Of Project Directories

Directory |Role
------------------|----------------
src/Comm           |Shared code
src/ConfigServer   |Configuration service
src/DbAccess       |Data persistence service
src/KVCacheServer  |k-v storage engine
src/MKVCacheServer |k-k-row、list、set、zset storage engine
src/OptServer      |Service deployment, operation and maintenance management, this module serves the web management platform
src/PropertyServer |Monitor information reporting service
src/Proxy          |Proxy service
src/Router         |Routing service
src/TarsComm       |Tars data structures
src/thirdParty     |Third party libraries

* ```docs-en``` : Documentation

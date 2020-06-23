# Compiling and deploy DCache Services

## Dependence
Install Tars development environment and Tars web before compiling DCache, since it is based on [Tars](https://github.com/TarsCloud/Tars) framework.

## Compilation

In source director：`mkdir build; cd build; cmake ..; make; make release; make tar`

to generate a Tars release package for each service.

## Depoly Public Services

Deploy OptServer, ConfigServer and PropertyServer on Tars web, APP name must be "DCache", to get each server's configuration information, please refer to [server_config_example](server_config_example-en.md)

## Install DCache web

DCache web is a module of Tars web, please get details from ["how to install DCache web"](https://github.com/TarsCloud/TarsWeb) 

## Deploy DCache APP and module

step1: Create a APP on DCache web.

step2: Deploy Router and Proxy, Router and Proxy serve all modules of a APP.

step3: Deploy Cache module. KVCache module is for key-value, MKVCache module is for k-k-row, list, set, zset.

to be continue...
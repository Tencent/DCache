#!/bin/bash

#公共函数
function LOG_ERROR()
{
	local msg=$(date +%Y-%m-%d" "%H:%M:%S);

    msg="${msg} $@";

	echo -e "\033[31m$msg \033[0m";	
}

function LOG_WARNING()
{
	local msg=$(date +%Y-%m-%d" "%H:%M:%S);

    msg="${msg} $@";

	echo -e "\033[33m$msg \033[0m";	
}

function LOG_DEBUG()
{
	local msg=$(date +%Y-%m-%d" "%H:%M:%S);

    msg="${msg} $@";

 	echo -e "\033[40;37m$msg \033[0m";	
}

function LOG_INFO()
{
	local msg=$(date +%Y-%m-%d" "%H:%M:%S);
	
	for p in $@
	do
		msg=${msg}" "${p};
	done
	
	echo -e "\033[32m$msg \033[0m"  	
}

if [ $# -lt 8 ]; then
    echo "$0 DCACHE_MYSQL_IP DCACHE_MYSQL_PORT DCACHE_MYSQL_USER DCACHE_MYSQL_PASSWORD CREATE(true/false) WEB_HOST WEB_TOKEN NODE_IP";
    exit 1
fi

DCACHE_MYSQL_IP=$1
DCACHE_MYSQL_PORT=$2
DCACHE_MYSQL_USER=$3
DCACHE_MYSQL_PASSWORD=$4
CREATE=$5
WEB_HOST=${6}
WEB_TOKEN=${7}
NODE_IP=${8}

if [ "$CREATE" != "true" ]; then
	CREATE="false"
fi

WORKDIR=$(cd $(dirname $0); pwd)

OS=`uname`

if [[ "$OS" =~ "Darwin" ]]; then
    OS=1
else
    OS=2
fi

LOG_INFO "====================================================================";
LOG_INFO "===**********************dcache-install**************************===";
LOG_INFO "====================================================================";

#输出配置信息
LOG_DEBUG "===>print config info >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
LOG_DEBUG "PARAMS:                  "$*
LOG_DEBUG "DCACHE_MYSQL_IP:         "$DCACHE_MYSQL_IP
LOG_DEBUG "DCACHE_MYSQL_PORT:       "$DCACHE_MYSQL_PORT
LOG_DEBUG "DCACHE_MYSQL_USER:       "${DCACHE_MYSQL_USER}
LOG_DEBUG "DCACHE_MYSQL_PASSWORD:   "${DCACHE_MYSQL_PASSWORD}
LOG_DEBUG "CREATE:                  "${CREATE}
LOG_DEBUG "WEB_HOST:                "${WEB_HOST}
LOG_DEBUG "WEB_TOKEN:               "${WEB_TOKEN}
LOG_DEBUG "WORKDIR:                 "${WORKDIR}
LOG_DEBUG "NODE_IP:                 "${NODE_IP}
LOG_DEBUG "OS:                      "${OS}
LOG_DEBUG "===<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< print config info finish.\n";

cmake .. -DTARS_WEB_HOST=${WEB_HOST} -DTARS_TOKEN=${WEB_TOKEN}

make tar

function exec_dcache()
{
    mysql -h${DCACHE_MYSQL_IP}  -u${DCACHE_MYSQL_USER} -P${DCACHE_MYSQL_PORT} -p${DCACHE_MYSQL_PASSWORD} -e "$1"

    ret=$?
    LOG_DEBUG "exec_dcache $1, ret: $ret"

    return $ret
}

sed_paramter=""
if [ "$OS" == 1 ]; then
    sed_paramter="\"\""
fi


function build_server_adapter()
{
    LOG_INFO "===>install DCacheOptServer:";
    sed -i ${sed_paramter} "s/host_ip/$NODE_IP/g" assets_tmp/DCacheOptServer.json
    curl -s -X POST -H "Content-Type: application/json" ${WEB_HOST}/api/deploy_server?ticket=${WEB_TOKEN} -d@assets_tmp/DCacheOptServer.json
    echo
    LOG_DEBUG

    LOG_INFO "===>install DCacheConfigServer:";
    sed -i ${sed_paramter} "s/host_ip/$NODE_IP/g" assets_tmp/ConfigServer.json
    curl -s -X POST -H "Content-Type: application/json" ${WEB_HOST}/api/deploy_server?ticket=${WEB_TOKEN} -d@assets_tmp/ConfigServer.json
    echo
    LOG_DEBUG

    LOG_INFO "===>install PropertyServer:";
    sed -i ${sed_paramter} "s/host_ip/$NODE_IP/g" assets_tmp/PropertyServer.json
    curl -s -X POST -H "Content-Type: application/json" ${WEB_HOST}/api/deploy_server?ticket=${WEB_TOKEN} -d@assets_tmp/PropertyServer.json
    echo
    LOG_DEBUG
}

function update_conf()
{
    sed -i ${sed_paramter} "s/dcache_host/$DCACHE_MYSQL_IP/g" $1
    sed -i ${sed_paramter} "s/dcache_user/$DCACHE_MYSQL_USER/g" $1
    sed -i ${sed_paramter} "s/dcache_port/$DCACHE_MYSQL_PORT/g" $1
    sed -i ${sed_paramter} "s/dcache_pass/$DCACHE_MYSQL_PASSWORD/g" $1
}

function build_server_conf()
{
    LOG_INFO "===>install DCacheOptServer.conf:";
	update_conf config_tmp/DCacheOptServer.conf
    curl -s -X POST -H "Content-Type: application/json" ${WEB_HOST}/api/add_config_file?ticket=${WEB_TOKEN} -d@config_tmp/DCacheOptServer.conf
    echo
    LOG_DEBUG

    LOG_INFO "===>install ConfigServer.conf:";
	update_conf config_tmp/ConfigServer.conf
    curl -s -X POST -H "Content-Type: application/json" ${WEB_HOST}/api/add_config_file?ticket=${WEB_TOKEN} -d@config_tmp/ConfigServer.conf
    echo
    LOG_DEBUG

    LOG_INFO "===>install PropertyServer.conf:";
	update_conf config_tmp/PropertyServer.conf
    curl -s -X POST -H "Content-Type: application/json" ${WEB_HOST}/api/add_config_file?ticket=${WEB_TOKEN} -d@config_tmp/PropertyServer.conf
    echo
    LOG_DEBUG

}


cp -rf ../deploy/config config_tmp
cp -rf ../deploy/assets assets_tmp

build_server_adapter

build_server_conf

rm -rf config_tmp
rm -rf assets_tmp

if [ "${CREATE}" == "true" ]; then 
	exec_dcache "create database if not exists db_dcache_relation"
	exec_dcache "create database if not exists db_dcache_property"
	mysql -h${DCACHE_MYSQL_IP}  -u${DCACHE_MYSQL_USER} -P${DCACHE_MYSQL_PORT} -p${DCACHE_MYSQL_PASSWORD} --default-character-set=utf8mb4 db_dcache_relation < ../deploy/sql/db_dcache_relation.sql
fi

#上传发布包
LOG_DEBUG "curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@DConfigServer.tgz -Fapplication=DCache -Fmodule_name=ConfigServer -Fcomment=dcache-install"
curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@ConfigServer.tgz -Fapplication=DCache -Fmodule_name=ConfigServer -Fcomment=dcache-install

echo 
LOG_DEBUG "curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@DCacheOptServer.tgz -Fapplication=DCache -Fmodule_name=DCacheOptServer -Fcomment=dcache-install"
curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@DCacheOptServer.tgz -Fapplication=DCache -Fmodule_name=DCacheOptServer -Fcomment=dcache-install 

echo 
LOG_DEBUG "curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@PropertyServer.tgz -Fapplication=DCache -Fmodule_name=PropertyServer -Fcomment=dcache-install"
curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@PropertyServer.tgz -Fapplication=DCache -Fmodule_name=PropertyServer -Fcomment=dcache-install

echo 
LOG_DEBUG "curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@RouterServer.tgz -Fapplication=DCache -Fmodule_name=RouterServer -Fcomment=dcache-install"
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@RouterServer.tgz -Fapplication=DCache -Fmodule_name=RouterServer -Fcomment=dcache-install

echo 
LOG_DEBUG "curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@CombinDbAccessServer.tgz -Fapplication=DCache -Fmodule_name=CombinDbAccessServer -Fcomment=dcache-install"
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@CombinDbAccessServer.tgz -Fapplication=DCache -Fmodule_name=CombinDbAccessServer -Fcomment=dcache-install

echo 
LOG_DEBUG "curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@ProxyServer.tgz -Fapplication=DCache -Fmodule_name=ProxyServer -Fcomment=dcache-install"
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@ProxyServer.tgz -Fapplication=DCache -Fmodule_name=ProxyServer -Fcomment=dcache-install

echo 
LOG_DEBUG "curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@KVCacheServer.tgz -Fapplication=DCache -Fmodule_name=KVCacheServer -Fcomment=dcache-install"
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@KVCacheServer.tgz -Fapplication=DCache -Fmodule_name=KVCacheServer -Fcomment=dcache-install 

echo 
LOG_DEBUG "curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@MKVCacheServer.tgz -Fapplication=DCache -Fmodule_name=MKVCacheServer -Fcomment=dcache-install"
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@MKVCacheServer.tgz -Fapplication=DCache -Fmodule_name=MKVCacheServer -Fcomment=dcache-install 

echo 


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

#手工创建: DCacheOptServer/ConfigServer/PropertyServer

#yum install mysql
#输入参数: tars数据库地址, dcache数据库地址

if [ $# -lt 10 ]; then
    echo "$0 TARS_MYSQL_IP TARS_MYSQL_PORT TARS_MYSQL_USER TARS_MYSQL_PASSWORD DCACHE_MYSQL_IP DCACHE_MYSQL_PORT DCACHE_MYSQL_USER DCACHE_MYSQL_PASSWORD CREATE(true/false) WEB_HOST WEB_TOKEN";
    exit 1
fi

TARS_MYSQL_IP=$1
TARS_MYSQL_PORT=$2
TARS_MYSQL_USER=$3
TARS_MYSQL_PASSWORD=$4
DCACHE_MYSQL_IP=$5
DCACHE_MYSQL_PORT=$6
DCACHE_MYSQL_USER=$7
DCACHE_MYSQL_PASSWORD=$8
CREATE=$9
WEB_HOST=${10}
WEB_TOKEN=${11}

if [ "$CREATE" != "true" ]; then
	CREATE="false"
fi

WORKDIR=$(cd $(dirname $0); pwd)

LOG_INFO "====================================================================";
LOG_INFO "===**********************dcache-install**************************===";
LOG_INFO "====================================================================";

#输出配置信息
LOG_DEBUG "===>print config info >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
LOG_DEBUG "PARAMS:                  "$*
LOG_DEBUG "TARS_MYSQL_IP:           "$TARS_MYSQL_IP 
LOG_DEBUG "TARS_MYSQL_PORT:         "$TARS_MYSQL_PORT
LOG_DEBUG "TARS_MYSQL_USER:         "$TARS_MYSQL_USER
LOG_DEBUG "TARS_MYSQL_PASSWORD:     "$TARS_MYSQL_PASSWORD
LOG_DEBUG "DCACHE_MYSQL_IP:         "$DCACHE_MYSQL_IP
LOG_DEBUG "DCACHE_MYSQL_PORT:       "$DCACHE_MYSQL_PORT
LOG_DEBUG "DCACHE_MYSQL_USER:       "${DCACHE_MYSQL_USER}
LOG_DEBUG "DCACHE_MYSQL_PASSWORD:   "${DCACHE_MYSQL_PASSWORD}
LOG_DEBUG "CREATE:                  "${CREATE}
LOG_DEBUG "WEB_HOST:                "${WEB_HOST}
LOG_DEBUG "WEB_TOKEN:               "${WEB_TOKEN}
LOG_DEBUG "WORKDIR:                 "${WORKDIR}
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

if [ "${CREATE}" == "true" ]; then 
exec_dcache "create database db_dcache_relation"
exec_dcache "create database db_dcache_property"
mysql -h${DCACHE_MYSQL_IP}  -u${DCACHE_MYSQL_USER} -P${DCACHE_MYSQL_PORT} -p${DCACHE_MYSQL_PASSWORD} --default-character-set=utf8mb4 db_dcache_relation < ../deploy/sql/db_dcache_relation_create.sql
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
LOG_DEBUG "curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@KVCacheServer.tgz -Fapplication=DCache -Fmodule_name=DCacheServerGroup -Fcomment=dcache-install -Fpackage_type=1"
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@KVCacheServer.tgz -Fapplication=DCache -Fmodule_name=DCacheServerGroup -Fcomment=dcache-install -Fpackage_type=1

echo 
LOG_DEBUG "curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@MKVCacheServer.tgz -Fapplication=DCache -Fmodule_name=DCacheServerGroup -Fcomment=dcache-install -Fpackage_type=2"
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@MKVCacheServer.tgz -Fapplication=DCache -Fmodule_name=DCacheServerGroup -Fcomment=dcache-install -Fpackage_type=2

echo 

cp -rf ../deploy/config config_tmp

bin/mysql-tool --config=config_tmp --dcache_host=${DCACHE_MYSQL_IP}  --dcache_user=${DCACHE_MYSQL_USER} --dcache_port=${DCACHE_MYSQL_PORT} --dcache_pass=${TARS_MYSQL_PASSWORD} --tars_host=${TARS_MYSQL_IP} --tars_user=${TARS_MYSQL_USER} --tars_port=${TARS_MYSQL_PORT} --tars_pass=${TARS_MYSQL_PASSWORD}

rm -rf config_tmp

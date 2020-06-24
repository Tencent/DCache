#!/bin/bash

#公共函数
function LOG_ERROR()
{
	local msg=$(date +%Y-%m-%d" "%H:%M:%S);

    msg="${msg} $@";

	echo -e "\033[31m $msg \033[0m";	
}

function LOG_WARNING()
{
	local msg=$(date +%Y-%m-%d" "%H:%M:%S);

    msg="${msg} $@";

	echo -e "\033[33m $msg \033[0m";	
}

function LOG_DEBUG()
{
	local msg=$(date +%Y-%m-%d" "%H:%M:%S);

    msg="${msg} $@";

 	echo -e "\033[40;37m $msg \033[0m";	
}

function LOG_INFO()
{
	local msg=$(date +%Y-%m-%d" "%H:%M:%S);
	
	for p in $@
	do
		msg=${msg}" "${p};
	done
	
	echo -e "\033[32m $msg \033[0m"  	
}

#手工创建: DCacheOptServer/ConfigServer/PropertyServer

#yum install mysql
#输入参数: tars数据库地址, dcache数据库地址

if [ $# -lt 9 ]; then
    echo "$0 TARS_MYSQL_IP TARS_MYSQL_PORT TARS_MYSQL_USER TARS_MYSQL_PASSWORD DCACHE_MYSQL_IP DCACHE_MYSQL_PORT DCACHE_MYSQL_USER DCACHE_MYSQL_PASSWORD WEB_HOST WEB_TOKEN";
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
WEB_HOST=$9
WEB_TOKEN=$10

WORKDIR=$(cd $(dirname $0); pwd)

cmake .. -DTARS_WEB_HOST=${WEB_HOST} -DTARS_TOKEN=${WEB_TOKEN}

make tar

function exec_dcache()
{
    mysql -h${DCACHE_MYSQL_IP}  -u${DCACHE_MYSQL_USER} -P${DCACHE_MYSQL_PORT} -p${DCACHE_MYSQL_PASSWORD} -e "$1"

    ret=$?
    LOG_DEBUG "exec_dcache $1, ret: $ret"

    return $ret
}

#执行建库命令
exec_dcache "create database db_dcache_property"

exec_dcache "create database db_dcache_relation"
mysql -h${DCACHE_MYSQL_IP}  -u${DCACHE_MYSQL_USER} -P${DCACHE_MYSQL_PORT} -p${DCACHE_MYSQL_PASSWORD} --default-character-set=utf8mb4 db_dcache_relation < ../deploy/sql/db_dcache_relation.sql

#授权
exec_dcache "grant all on *.* to 'dcache'@'%' identified by 'dcache@2019' with grant option;"
exec_dcache "grant all on *.* to 'dcache'@'localhost' identified by 'dcache@2019' with grant option;"
exec_dcache "flush privileges;"

#上传发布包
curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@ConfigServer.tgz -Fapplication=DCache -Fmodule_name=ConfigServer -Fcomment=dcache-install
curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@DCacheOptServer.tgz -Fapplication=DCache -Fmodule_name=DCacheOptServer -Fcomment=dcache-install 
curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@PropertyServer.tgz -Fapplication=DCache -Fmodule_name=PropertyServer -Fcomment=dcache-install
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@RouterServer.tgz -Fapplication=DCache -Fmodule_name=RouterServer -Fcomment=dcache-install
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@ProxyServer.tgz -Fapplication=DCache -Fmodule_name=ProxyServer -Fcomment=dcache-install
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@DG-KVCacheServer.tgz -Fapplication=DCache -Fmodule_name=DCacheServerGroup -Fcomment=dcache-install -Fpackage_type=1
curl ${WEB_HOST}/api/upload_patch_package?ticket=${WEB_TOKEN} -Fsuse=@DG-MKVCacheServer.tgz -Fapplication=DCache -Fmodule_name=DCacheServerGroup -Fcomment=dcache-install -Fpackage_type=2

#修改配置文件内的ip地址并写库
cp -rf ../deploy/config config_tmp

bin/mysql-tool --config=config_tmp --dcache_host=${DCACHE_MYSQL_IP}  --dcache_user=${DCACHE_MYSQL_USER} --dcache_port=${DCACHE_MYSQL_PORT} --dcache_pass=${TARS_MYSQL_PASSWORD} --tars_host=${TARS_MYSQL_IP} --tars_user=${TARS_MYSQL_USER} --tars_port=${TARS_MYSQL_PORT} --tars_pass=${TARS_MYSQL_PASSWORD}

rm -rf config_tmp

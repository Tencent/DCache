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

if [ $# -lt 7 ]; then
    echo "$0 DCACHE_MYSQL_IP DCACHE_MYSQL_PORT DCACHE_MYSQL_USER DCACHE_MYSQL_PASSWORD WEB_HOST WEB_TOKEN NODE_IP";
    exit 1
fi

DCACHE_MYSQL_IP=$1
DCACHE_MYSQL_PORT=$2
DCACHE_MYSQL_USER=$3
DCACHE_MYSQL_PASSWORD=$4
WEB_HOST=${5}
WEB_TOKEN=${6}
NODE_IP=${7}

WORKDIR=$(cd $(dirname $0); pwd)

APP=DCache

TARGET=DCacheWebServer

OS=`uname`

if [[ "$OS" =~ "Darwin" ]]; then
    OS=1
else
    OS=2
fi

LOG_INFO "====================================================================";
LOG_INFO "===**********************${TARGET}-install**************************===";
LOG_INFO "====================================================================";

#输出配置信息
LOG_DEBUG "===>print config info >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
LOG_DEBUG "PARAMS:                  "$*
LOG_DEBUG "DCACHE_MYSQL_IP:         "$DCACHE_MYSQL_IP
LOG_DEBUG "DCACHE_MYSQL_PORT:       "$DCACHE_MYSQL_PORT
LOG_DEBUG "DCACHE_MYSQL_USER:       "${DCACHE_MYSQL_USER}
LOG_DEBUG "DCACHE_MYSQL_PASSWORD:   "${DCACHE_MYSQL_PASSWORD}
LOG_DEBUG "WEB_HOST:                "${WEB_HOST}
LOG_DEBUG "WEB_TOKEN:               "${WEB_TOKEN}
LOG_DEBUG "WORKDIR:                 "${WORKDIR}
LOG_DEBUG "NODE_IP:                 "${NODE_IP}
LOG_DEBUG "OS:                      "${OS}
LOG_DEBUG "===<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< print config info finish.\n";

function build_server_adapter()
{
    LOG_INFO "===>install DCacheWebServer:";
    sed -i ${sed_paramter} "s/host_ip/$NODE_IP/g" assets_tmp/DCacheWebServer.json
    curl -s -X POST -H "Content-Type: application/json" ${WEB_HOST}/api/deploy_server?ticket=${WEB_TOKEN} -d@assets_tmp/DCacheWebServer.json
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
    LOG_INFO "===>install config.json:";
	  update_conf config_tmp/DCacheWebServer.json
    curl -s -X POST -H "Content-Type: application/json" ${WEB_HOST}/api/add_config_file?ticket=${WEB_TOKEN} -d@config_tmp/DCacheWebServer.json
    echo
    LOG_DEBUG
}

cd ..

mkdir -p tmp

TARGET_PATH=tmp/${TARGET}

echo "mkdir -p ${TARGET_PATH}"
mkdir -p ${TARGET_PATH}

echo "rm old build & copy new build"
rm -rf ${TARGET_PATH}/dist

echo "copy package.json"
cp package.json ${TARGET_PATH}/
cp -rf src ${TARGET_PATH}/

mkdir -p ${TARGET_PATH}/client

cp -rf client/dist ${TARGET_PATH}/client

cd ${TARGET_PATH}
echo "npm install"
npm install

mkdir -p tars_nodejs

cp -rf node_modules/@tars/node-agent tars_nodejs
cp -rf node_modules/* tars_nodejs/node-agent/node_modules

cd ..

echo "tar czf ${TARGET}.tgz"
tar czf ${TARGET}.tgz ${TARGET}

cd ..
cp -rf deploy/config config_tmp
cp -rf deploy/assets assets_tmp

build_server_adapter

build_server_conf

rm -rf config_tmp
rm -rf assets_tmp

cd tmp

LOG_DEBUG "curl ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@${TARGET}.tgz -Fapplication=${APP} -Fmodule_name=${TARGET}  -Fcomment=dcache-install"
# curl --progress-bar ${TARS_K8S_WEB_HOST}/pages/k8s/api/upload_and_publish?ticket=${TARS_K8S_TOKEN} -Fsuse=@${TARGET}.tgz -Fapplication=${APP} -Fmodule_name=${TARGET} -Fserver_type=nodejs  -Fbase_image=${TARS_K8S_BASE_IMAGE} -Fcomment=upload
curl --progress-bar ${WEB_HOST}/api/upload_and_publish?ticket=${WEB_TOKEN} -Fsuse=@${TARGET}.tgz -Fapplication=${APP} -Fmodule_name=${TARGET}  -Fcomment=dcache-install

echo ""

cd ..

#!/bin/bash

#TARS_K8S_WEB_HOST="http://127.0.0.1:3000"
TARS_K8S_TOKEN="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOiJhZG1pbiIsImlhdCI6MTYzMjQ2MTg3NywiZXhwIjoxNzE4MDg0Mjc3fQ.2WqnAoNWS0qYXbSn_OuL18a59R4nkxrjwIeDjGii3QU"
#TARS_TOKEN="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOiJhZG1pbiIsImlhdCI6MTY0NjcyMTcxOCwiZXhwIjoxNzM5MjU2MTE4fQ.k6m3q4J3bR48vlkig3-T4QJvgJOwUyrlEyMS39xk554"
TARS_K8S_BASE_IMAGE="tarscloud/tars.nodejsbase"

TARS_K8S_WEB_HOST=$1
TARS_TOKEN=$2

APP=DCache

TARGET=DCacheWebServer

TARGET_PATH=tmp/${TARGET}

mkdir -p tmp

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
echo "npm install --omit=dev"
npm install --omit=dev

mkdir -p tars_nodejs

cp -rf node_modules/@tars/node-agent tars_nodejs

cd ..

echo "tar czf ${TARGET}.tgz"
tar czf ${TARGET}.tgz ${TARGET}

# curl --progress-bar ${TARS_K8S_WEB_HOST}/pages/k8s/api/upload_and_publish?ticket=${TARS_K8S_TOKEN} -Fsuse=@${TARGET}.tgz -Fapplication=${APP} -Fmodule_name=${TARGET} -Fserver_type=nodejs  -Fbase_image=${TARS_K8S_BASE_IMAGE} -Fcomment=upload
curl --progress-bar ${TARS_K8S_WEB_HOST}/api/upload_and_publish?ticket=${TARS_TOKEN} -Fsuse=@${TARGET}.tgz -Fapplication=${APP} -Fmodule_name=${TARGET}  -Fcomment=upload

echo ""

cd ..

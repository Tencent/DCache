/**
* Tencent is pleased to support the open source community by making DCache available.
* Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
* Licensed under the BSD 3-Clause License (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSD-3-Clause
*
* Unless required by applicable law or agreed to in writing, software distributed under
* the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
* either express or implied. See the License for the specific language governing permissions
* and limitations under the License.
*/
#include "ConfigServer.h"
#include "ConfigImp.h"

void ConfigServer::initialize()
{
    //加载配置文件
    addConfig("ConfigServer.conf");
    
    //增加对象
    addServant<ConfigImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".ConfigObj");
}

void ConfigServer::destroyApp()
{
    TLOG_DEBUG("ConfigServer::destroyApp succ." << endl);
}

int main(int argc, char *argv[])
{
    try
    {
        ConfigServer app;

        app.main(argc, argv);

        app.waitForShutdown();
    }
    catch(exception &ex)
    {
        cerr<< ex.what() << endl;
    }

    return 0;
}
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
#include "DbAccessServer.h"
#include "DbAccessImp.h"

DbAccessServer g_app;

/////////////////////////////////////////////////////////////////
void
DbAccessServer::initialize()
{
    //initialize application here:
    //...
    addConfig(ServerConfig::ServerName + ".conf");
    addServant<DbAccessImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".DbAccessObj");

    TARS_ADD_ADMIN_CMD_NORMAL("ver", DbAccessServer::showVer);

    TC_Config tcConf;
    tcConf.parseFile(ServerConfig::BasePath + ServerConfig::ServerName + ".conf");
}

bool DbAccessServer::showVer(const string& command, const string& params, string& result)
{
    result = "DBAccess_1.1";
    return true;
}

/////////////////////////////////////////////////////////////////
void
DbAccessServer::destroyApp()
{
    //destroy application here:
    //...
}
/////////////////////////////////////////////////////////////////
int
main(int argc, char* argv[])
{
    try
    {
        g_app.main(argc, argv);
        g_app.waitForShutdown();
    }
    catch (std::exception& e)
    {
        cerr << "std::exception:" << e.what() << std::endl;
    }
    catch (...)
    {
        cerr << "unknown exception." << std::endl;
    }
    return -1;
}
/////////////////////////////////////////////////////////////////

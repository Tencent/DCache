/**
 * Tencent is pleased to support the open source community by making Tars available.
 *
 * Copyright (C) 2016THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use this file except 
 * in compliance with the License. You may obtain a copy of the License at
 *
 * https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software distributed 
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR 
 * CONDITIONS OF ANY KIND, either express or implied. See the License for the 
 * specific language governing permissions and limitations under the License.
 */

#include "util/tc_mysql.h"
#include "util/tc_option.h"
#include "util/tc_file.h"
#include "util/tc_common.h"
#include "util/tc_config.h"
#include <iostream>
#include <string>

using namespace tars;
using namespace std;

/**
 * mysql-tool --h --u --p --P --check
 * mysql-tool --h --u --p --P --sql
 * mysql-tool --h --u --p --P --file
 * @param argc
 * @param argv
 * @return
 */

struct MysqlCommand
{
	void uploadConfig(TC_Option &option)
	{
		map<string, string> mSrcDest;
		mSrcDest["${dcache_host}"] = option.getValue("dcache_host");
		mSrcDest["${dcache_user}"] = option.getValue("dcache_user");
		mSrcDest["${dcache_port}"] = option.getValue("dcache_port");
		mSrcDest["${dcache_pass}"] = option.getValue("dcache_pass");
		mSrcDest["${tars_host}"] = option.getValue("tars_host");
		mSrcDest["${tars_user}"] = option.getValue("tars_user");
		mSrcDest["${tars_port}"] = option.getValue("tars_port");
		mSrcDest["${tars_pass}"] = option.getValue("tars_pass");

	    TC_Mysql mysql;

	    mysql.init(option.getValue("tars_host"), option.getValue("tars_user"), option.getValue("tars_pass"), "db_tars", "utf8mb4", TC_Common::strto<int>(option.getValue("tars_port")), CLIENT_MULTI_STATEMENTS);

		vector<string> configs;
		TC_File::listDirectory(option.getValue("config"), configs, false);

		for(auto config : configs)
		{
			string content = TC_File::load2str(config);

			content = TC_Common::replace(content, mSrcDest);

			string fileName = TC_File::extractFileName(config);
			map<string, pair<TC_Mysql::FT, string> > data;
			data["server_name"] = make_pair(TC_Mysql::DB_STR, "DCache." + TC_File::excludeFileExt(fileName));
			data["filename"] 	= make_pair(TC_Mysql::DB_STR, fileName);
			data["config"] 		= make_pair(TC_Mysql::DB_STR, content);
			data["level"]		= make_pair(TC_Mysql::DB_INT, "2");
			data["posttime"]	= make_pair(TC_Mysql::DB_INT, "now()");

			// cout << "-----------------------------------------------" << endl;
			// cout << fileName << endl;
			// cout << content << endl;
			// cout << mysql.escapeString(content) << endl;

			mysql.replaceRecord("t_config_files", data);
		}
	}

};

int main(int argc, char *argv[])
{
	TC_Option option;

    try
    {
		option.decode(argc, argv);

	    MysqlCommand mysqlCmd;

	    if(option.hasParam("config"))
	    {
		    mysqlCmd.uploadConfig(option);
	    }
	    else
	    {
	    	assert(false);
	    }
    }
    catch(exception &ex)
    {
		cout << "exec mysql parameter: " << TC_Common::tostr(option.getMulti().begin(), option.getMulti().end()) << endl;
		cout << "error: " << ex.what() << endl;
        exit(-1);
    }

    return 0;
}



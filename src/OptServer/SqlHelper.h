#ifndef DCACHE_OPT_SQL_HELPER_H
#define DCACHE_OPT_SQL_HELPER_H

#include "util/tc_mysql.h"

namespace DCacheSql
{
inline std::string quote(tars::TC_Mysql &mysql, const std::string &value)
{
    return "'" + mysql.escapeString(value) + "'";
}
}

#endif

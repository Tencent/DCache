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
#ifndef _STRING_UTIL_H_
#define _STRING_UTIL_H_
#include <zlib.h>
#include <string>
#include <vector>
#include <assert.h>
#include <netinet/in.h>

using namespace std;

class StringUtil
{
public:
    StringUtil() {}

    ~StringUtil() {}

    /**
     * 对字符串进行gzip压缩
     * @param src
     * @param length
     * @param buffer
     *
     * @return bool
     */
    static bool gzipCompress(const char* src, size_t length, string& buffer);

    /**
     * 对字符串进行gzip解压
     * @param src
     * @param length
     * @param buffer
     *
     * @return int
     */
    static bool gzipUncompress(const char *src, size_t length, string &buffer);


    /*
    * 解析字符串
    */
    static bool parseString(const string& str, vector<string>& res);

public:

    const static size_t GZIP_MIN_STR_LEN;
};

#endif

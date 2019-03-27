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
#include "EtcdHttp.h"
#include <assert.h>
#include "global.h"

using namespace tars;

EtcdHttpRequest::EtcdHttpRequest(const std::string &url, enum HttpMethod method)
    : _urlSetKeyValue(false), _bodySetKeyValue(false), _reqUrl(url + "/"), _method(method)
{
}

int EtcdHttpRequest::doRequest(int timeout, std::string *respContent)
{
    setHttpRequest();
    tars::TC_HttpResponse resp;
    int rc = _httpReq.doRequest(resp, timeout);
    if (rc != 0) return rc;
    *respContent = resp.getContent();
    return 0;
}

void EtcdHttpRequest::setHttpRequest()
{
    if (_method == HTTP_GET)
    {
        TLOGDEBUG(FILE_FUN << "get request url : " << _reqUrl << endl);
        _httpReq.setGetRequest(_reqUrl);
    }
    else if (_method == HTTP_PUT)
    {
        TLOGDEBUG(FILE_FUN << "put request url : " << _reqUrl << "  body : " << _body << endl);
        _httpReq.setPutRequest(_reqUrl, _body);
    }
}

int EtcdHttpParser::parseContent(const std::string &httpContent)
{
    _content = httpContent;
    rapidjson::ParseResult ok = _document.Parse(_content.c_str());
    return ok ? 0 : -1;
}

int EtcdHttpParser::getEtcdError(int *errCode, std::string *message) const
{
    assert(errCode);
    assert(message);

    rapidjson::Value::ConstMemberIterator it = _document.FindMember("errorCode");
    if (it != _document.MemberEnd() && it->value.IsNumber())
    {
        assert(_document.HasMember("message"));
        assert(_document["message"].IsString());
        *message = _document["message"].GetString();
        *errCode = _document["errorCode"].GetInt();
        return -1;
    }
    return 0;
}

int EtcdHttpParser::getCurrentNodeValue(std::string *value) const
{
    assert(value);

    rapidjson::Value::ConstMemberIterator it = _document.FindMember("node");
    if (it == _document.MemberEnd() || !it->value.IsObject())
    {
        return -1;
    }

    rapidjson::Value::ConstMemberIterator it2 = it->value.FindMember("value");
    if (it2 == it->value.MemberEnd() || !it2->value.IsString())
    {
        return -1;
    }

    *value = it2->value.GetString();
    return 0;
}

int EtcdHttpParser::getCurrentNodeKey(std::string *key) const
{
    assert(key);

    rapidjson::Value::ConstMemberIterator it = _document.FindMember("node");
    if (it == _document.MemberEnd() || !it->value.IsObject())
    {
        return -1;
    }

    rapidjson::Value::ConstMemberIterator it2 = it->value.FindMember("key");
    if (it2 == it->value.MemberEnd() || !it2->value.IsString())
    {
        return -1;
    }

    *key = it2->value.GetString();
    return 0;
}

int EtcdHttpParser::getAction(std::string *action) const
{
    rapidjson::Value::ConstMemberIterator it = _document.FindMember("action");
    if (it == _document.MemberEnd() || !it->value.IsString())
    {
        return -1;
    }

    *action = it->value.GetString();
    return 0;
}

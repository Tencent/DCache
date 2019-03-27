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
// 封装了ETCD的HTTP接口。具体的消息格式可以参考ETCD v2的API：
// https://coreos.com/etcd/docs/latest/v2/api.html

#ifndef __ETCDHTTP_H__
#define __ETCDHTTP_H__

#include <assert.h>
#include <string>
#include "global.h"
#include "rapidjson/document.h"
#include "servant/TarsLogger.h"
#include "util/tc_http.h"

// 默认的HTTP请求的超时时间(单位:毫秒)
const int DEFAULT_HTTP_REQUEST_TIMEOUT = 3000;
// ETCD watch命令的超时时间
const int ETCD_WATCH_REQUEST_TIMEOUT = 5000;

enum HttpMethod
{
    HTTP_GET = 0,
    HTTP_HEAD,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
};

class EtcdHttpRequest
{
public:
    EtcdHttpRequest(const std::string &url, enum HttpMethod method);

    // 设置默认的ETCD HTTP请求的头部
    void setDefaultHeader()
    {
        _httpReq.setHeader("Content-Type", "application/x-www-form-urlencoded");
    }

    // 设置请求的Key值，任何请求都应当先设置Key
    // 因为ETCD的Key是直接append在URL当中的，所以不需要指定HTTP方法。
    void setKey(const std::string &key)
    {
        assert(!_urlSetKeyValue);
        _reqUrl += key;
    }

    // 设置请求修改的Key所对应的Value值。一般PUT请求时用
    void setValue(const std::string &value)
    {
        assert(_method == HTTP_PUT);
        appendBodyKeyValue("value", value);
    }

    void setPrevValue(const std::string prevVal)
    {
        assert(_method == HTTP_PUT);
        appendBodyKeyValue("prevValue", prevVal);
    }

    // 检查当前键是否已经存在
    void setPrevExist(bool b)
    {
        assert(_method == HTTP_PUT);
        std::string s = b ? "true" : "false";
        appendBodyKeyValue("prevExist", s);
    }

    // 是否更新TTL
    void setRefresh(bool b)
    {
        assert(_method == HTTP_PUT);
        std::string s = b ? "true" : "false";
        appendBodyKeyValue("refresh", s);
    }

    void setWatchKey()
    {
        assert(_method == HTTP_GET);
        appendUrlKeyValue("wait", "true");
    }

    // 设置Key的超时时间。
    // 在调用此方法前要先setKey来设置Key
    void setKeyTTL(int ttl)
    {
        assert(_method == HTTP_PUT);
        appendBodyKeyValue("ttl", tars::TC_Common::tostr(ttl));
    }

    // 执行HTTP请求
    // 返回0成功，非0失败，具体值参见TC_ClientSocket声明
    virtual int doRequest(int timeout, std::string *respContent);

    void getHostPort(std::string &domain, uint32_t &port)
    {
        setHttpRequest();
        _httpReq.getHostPort(domain, port);
    }

    std::string dumpURL() { return _reqUrl; }

    std::string dumpRequestBody() { return _body; }

    tars::TC_HttpRequest &getTCHttpRequest()
    {
        setHttpRequest();
        return _httpReq;
    }

private:
    // 设置HTTP请求包
    void setHttpRequest();

    void appendUrlKeyValue(const std::string &key, const std::string &val)
    {
        addUrlConnector();
        _reqUrl += key + "=" + val;
    }

    void appendBodyKeyValue(const std::string &key, const std::string &val)
    {
        addBodyConnector();
        _body += key + "=" + val;
    }

    void addUrlConnector()
    {
        if (!_urlSetKeyValue)
        {
            _reqUrl += "?";
            _urlSetKeyValue = true;
        }
        else
        {
            _reqUrl += "&";
        }
    }

    void addBodyConnector()
    {
        if (!_bodySetKeyValue)
        {
            _bodySetKeyValue = true;
            return;
        }

        _body += "&";
    }

private:
    bool _urlSetKeyValue;           // 标记当前URL是否已经设置了key-value
    bool _bodySetKeyValue;          // 标记当前_content字段是否已经设置了key-value
    tars::TC_HttpRequest _httpReq;  // HTTP请求
    std::string _body;              // POST请求的内容
    std::string _reqUrl;            // 请求的URL
    enum HttpMethod _method;        // 请求所采用的HTTP方法
};

class EtcdHttpParser
{
public:
    // 解析ETCD响应的HTTP内容。
    int parseContent(const std::string &httpContent);

    // 获取ETCD的错误。
    // 没有错误时返回0。发生错误时设置错误码errCode和错误信息message并返回-1
    int getEtcdError(int *errCode, std::string *message) const;

    // 获取当前节点的value字段的值
    int getCurrentNodeValue(std::string *value) const;

    // 获取当前节点Key的值
    int getCurrentNodeKey(std::string *key) const;

    // 获取当前操作的action字段
    int getAction(std::string *action) const;

private:
    std::string _content;           // 需要解析的HTTP响应的内容
    rapidjson::Document _document;  // rapdijson解析JSON后生成的对象
};

#endif  // __ETCDHTTP_H__

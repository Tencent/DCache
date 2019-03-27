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
#ifndef __RIGUP_H_
#define __RIGUP_H_

#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <map>

using namespace std;

/**
 * @brief 配置项描述类
 */
struct ConfigItem
{
    string _id;
    string _item;
    string _path;
};

/**
 * @brief 树节点类
 */
struct TreeNode
{
    TreeNode();
    ~TreeNode();
    string _value;
    string _content;
    bool   _leaf;
    list<TreeNode*> _child;
};

/**
 *@brief 将配置项拼凑为类xml格式的字符串的类
 */
class Rigup
{
public:
    /**
     * 构造函数
     */
    Rigup();

    /**
     * 析构函数
     */
    ~Rigup();

public:
    /**
     * 初始化函数
     */
    void init();

    /**
     * 添加配置项和配置项的值
     */
    int enter(const vector<ConfigItem> &items, const map<string, string> &content);
    
    /**
     * 获取类xml格式的配置项字符串
     */
    const string& getString();

private:
    /**
     * 释放树
     * @inparam root: 根节点
     */
    void freeNode(TreeNode* root);

    /**
     * 将路径分割成层次目录
     * @inparam path: 路径
     * @outparam directorys: 按层次的目录列表
     */
    void splitPath(const string &path, list<string> &directorys);
    
    /**
     * 插入配置项
     * @inparam item: 配置项名
     * @inparam path: 配置项路径
     * @inparam content: 配置项的内容
     * @return bool: true成功，false失败
     */
    bool insertItem(const string &item, const string &path, const string &content);
    
    /**
     * 插入配置项节点
     * @inparam root: 根节点
     * @inparam node: 配置项节点
     * @inparam content: 配置项目录
     * @return bool: true成功，false失败
     */
    bool insertNode(TreeNode* root, TreeNode* node, list<string>& directorys);
    
    /**
     * 将配置项树转换成类xml格式的字符串
     * @inparam root: 根节点
     * @inparam depth: 树深度
     */
    void toString(TreeNode* root, int depth);
    
    string getIndentation(int depth);

private:
    
    TreeNode* _xmlRoot;
    
    string _xmlString;
};

#endif

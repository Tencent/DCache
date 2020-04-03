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
#include "RigUp.h"
#include "servant/Application.h"

TreeNode::TreeNode()
{
}

TreeNode::~TreeNode()
{
    //TODO: 递归释放内存
}

Rigup::Rigup()
{
    init();
}

Rigup::~Rigup()
{
    freeNode(_xmlRoot);
}

void Rigup::init()
{
    _xmlRoot = new(std::nothrow) TreeNode();
    _xmlRoot->_leaf	 = false;
    _xmlRoot->_value = "";
}

void Rigup::freeNode(TreeNode* root)
{
    if (root != NULL)
    {
        list<TreeNode*>::iterator it = root->_child.begin();
        while (it != root->_child.end())
        {
            if (!(*it)->_leaf)
            {
                freeNode(*it);
            }
            else
            {	
                delete *it;
                *it = NULL;
            }
            ++it;
        }
        delete root;
        root = NULL;
    }
}

int Rigup::enter(const vector<ConfigItem> &items, const map<string, string> &content)
{
    int iRet = 0;

    size_t iSize = items.size();
    for(size_t i = 0; i < iSize; ++i)
    {
        map<string, string>::const_iterator it = content.find(items[i]._id);
        if(it != content.end()) 
        {
            if(!insertItem(items[i]._item, items[i]._path, it->second))
            {
                TLOGERROR("Rigup::enter insert item:" << items[i]._item << "|path:" << items[i]._path << "|content:" << it->second << endl);
                iRet = -1;
            }
        } 
    }

    return iRet;
}

const string& Rigup::getString()
{
    _xmlString = "";
    
    toString(_xmlRoot, 0);

    return _xmlString;
}

void Rigup::splitPath(const string& path, list<string>& directorys)
{
    directorys.clear();

    size_t start = 0, pos = 0;
    while(pos < path.length())
    {
        while((pos < path.length()) && (path[pos] != '/'))
        {	
            ++pos;
        }
        directorys.push_back(path.substr(start, pos - start));
        if (pos >= path.length())
        {
            break;
        }
        start = ++ pos;
    }
}

bool Rigup::insertItem(const string& item, const string& path, const string& content)
{
    TreeNode* newNode = new TreeNode();
    newNode->_content = content;
    newNode->_value	  = item;
    newNode->_leaf    = true;

    list<string> directorys;
    splitPath(path, directorys);

    return insertNode(_xmlRoot, newNode, directorys);
}

bool Rigup::insertNode(TreeNode* root, TreeNode* node, list<string> &directorys)
{
    if(!root)
    {
        return false;
    }

    if(directorys.empty())
    {
        root->_child.push_back(node);
        return true;
    }

    string current = directorys.front();
    directorys.pop_front();
    list<TreeNode*>::iterator it = root->_child.begin();
    while (it != root->_child.end())
    {
        if ((*it)->_value == current)
        {
            return insertNode(*it, node, directorys);
        }
        ++it;
    }

    TreeNode* newNode = new TreeNode();
    newNode->_value = current;
    newNode->_leaf  = false;
    root->_child.push_back(newNode);

    return insertNode(newNode, node, directorys);
}

void Rigup::toString(TreeNode* root, int depth)
{
    if(!root)
    {
        return;
    }

    list<TreeNode*>::const_iterator it = root->_child.begin();
    list<TreeNode*>::const_iterator itEnd = root->_child.end();
    
    while (it != itEnd)
    {
        if((*it)->_leaf)
        {
            // 专门处理MKCache的配置项main/record/field
            if( (*it)->_value == "VirtualRecord") 
            {
                _xmlString += (getIndentation(depth) + (*it)->_content + "\n");
            }
            else
            {
                _xmlString += (getIndentation(depth) + (*it)->_value + "=" + (*it)->_content + "\n");
            }
        }
        else
        {
            _xmlString += (getIndentation(depth) + "<" + (*it)->_value + ">\n");
            toString(*it, depth + 1);
            _xmlString += (getIndentation(depth) + "</" + (*it)->_value + ">\n");
        }
        
        ++it;
    }
}

string Rigup::getIndentation(int depth)
{
    string result = "";

    while(depth--) 
    {
        result += "\t";
    }
    
    return result;
}

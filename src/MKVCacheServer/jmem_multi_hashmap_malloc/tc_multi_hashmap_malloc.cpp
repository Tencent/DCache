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
#include "tc_multi_hashmap_malloc.h"
#include "util/tc_pack.h"
#include "util/tc_common.h"
#include "util/tc_timeprovider.h"

#include "MKCacheUtil.h"

namespace tars
{

    //int TC_Multi_HashMap_Malloc::FailureRecover::_iRefCount = 0;

    int TC_Multi_HashMap_Malloc::MainKey::getDataByPos(char *ptr, int len, uint32_t pos)
    {
        assert(ptr != NULL);

        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            uint32_t iDataLen = getHeadPtr()->_iDataLen;
            if ((pos + len) >= iDataLen)
                return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;

            memcpy(ptr, getHeadPtr()->_cData + pos, len);
            return TC_Multi_HashMap_Malloc::RT_OK;
        }
        else
        {
            uint32_t iUseSize = getHeadPtr()->_iSize - sizeof(tagMainKeyHead);

            if (pos < iUseSize)
            {
                if ((pos + len) <= iUseSize)
                {//全部都在这了，可以直接返回
                    memcpy(ptr, getHeadPtr()->_cData + pos, len);
                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {//只有一部分
                    memcpy(ptr, getHeadPtr()->_cData + pos, iUseSize - pos);
                    ptr += iUseSize - pos;
                    len -= iUseSize - pos;
                    pos = 0;
                }
            }
            else
            {//都不在这里
                pos = pos - iUseSize;
            }

            tagChunkHead *pChunk = getChunkHead(getHeadPtr()->_iNextChunk);
            while (len > 0)
            {
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);

                if (pos < iUseSize)
                {
                    if ((pos + len) <= iUseSize)
                    {//全部都在这了，可以直接返回
                        memcpy(ptr, pChunk->_cData + pos, len);
                        return TC_Multi_HashMap_Malloc::RT_OK;
                    }
                    else
                    {//只有一部分
                        memcpy(ptr, pChunk->_cData + pos, iUseSize - pos);
                        ptr += iUseSize - pos;
                        len -= iUseSize - pos;
                        pos = 0;
                    }
                }
                else
                {
                    pos = pos - iUseSize;
                }

                if (!pChunk->_bNextChunk)
                {
                    //最后一个了，如果到这里数据还没有找到，那就有问题了
                    if (len < 0 || len > 0)
                    {
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;
                    }

                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {
                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::MainKey::setDataByPos(uint32_t data, uint32_t pos)
    {
        char *ptr = (char*)&data;
        int len = 4;
        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            uint32_t iDataLen = getHeadPtr()->_iDataLen;
            if ((pos + sizeof(uint32_t)) >= iDataLen)
                return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;

            _pMap->saveValue(getHeadPtr()->_cData + pos, data);
            return TC_Multi_HashMap_Malloc::RT_OK;
        }
        else
        {
            uint32_t iUseSize = getHeadPtr()->_iSize - sizeof(tagMainKeyHead);

            if (pos < iUseSize)
            {
                if ((pos + len) <= iUseSize)
                {//全部都在这了，可以直接返回
                    _pMap->saveValue(getHeadPtr()->_cData + pos, data);
                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {//只有一部分
                    uint8_t i8Data;
                    uint16_t i16Data;
                    switch (iUseSize - pos)
                    {
                    case 1:
                        i8Data = *(uint8_t*)ptr;
                        _pMap->saveValue(getHeadPtr()->_cData + pos, i8Data);
                        break;
                    case 2:
                        i16Data = *(uint16_t*)ptr;
                        _pMap->saveValue(getHeadPtr()->_cData + pos, i16Data);
                        break;
                    case 3:
                        i16Data = *(uint16_t*)ptr;
                        _pMap->saveValue(getHeadPtr()->_cData + pos, i16Data);
                        i8Data = *(uint8_t*)(ptr + sizeof(uint16_t));
                        _pMap->saveValue(getHeadPtr()->_cData + pos + sizeof(uint16_t), i8Data);
                        break;
                    default:
                        assert(false);
                    }
                    ptr += iUseSize - pos;
                    len -= iUseSize - pos;
                    pos = 0;
                }
            }
            else
            {//都不在这里
                pos = pos - iUseSize;
            }

            tagChunkHead *pChunk = getChunkHead(getHeadPtr()->_iNextChunk);
            while (len > 0)
            {
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);

                if (pos < iUseSize)
                {
                    uint8_t i8Data;
                    uint16_t i16Data;
                    uint32_t i32Data;
                    if ((pos + len) <= iUseSize)
                    {//全部都在这了，可以直接返回
                        switch (len)
                        {
                        case 1:
                            i8Data = *(uint8_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i8Data);
                            break;
                        case 2:
                            i16Data = *(uint16_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i16Data);
                            break;
                        case 3:
                            i16Data = *(uint16_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i16Data);
                            i8Data = *(uint8_t*)(ptr + sizeof(uint16_t));
                            _pMap->saveValue(pChunk->_cData + pos + sizeof(uint16_t), i8Data);
                            break;
                        case 4:
                            i32Data = *(uint32_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i32Data);
                            break;
                        default:
                            assert(false);
                        }
                        return TC_Multi_HashMap_Malloc::RT_OK;
                    }
                    else
                    {//只有一部分
                        switch (iUseSize - pos)
                        {
                        case 1:
                            i8Data = *(uint8_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i8Data);
                            break;
                        case 2:
                            i16Data = *(uint16_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i16Data);
                            break;
                        case 3:
                            i16Data = *(uint16_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i16Data);
                            i8Data = *(uint8_t*)(ptr + sizeof(uint16_t));
                            _pMap->saveValue(pChunk->_cData + pos + sizeof(uint16_t), i8Data);
                            break;
                        default:
                            assert(false);
                        }
                        ptr += iUseSize - pos;
                        len -= iUseSize - pos;
                        pos = 0;
                    }
                }
                else
                {
                    pos = pos - iUseSize;
                }

                if (!pChunk->_bNextChunk)
                {
                    //最后一个了，如果到这里数据还没有找到，那就有问题了
                    if (len < 0 || len > 0)
                    {
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;
                    }

                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {
                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }

            assert(len == 0);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    bool TC_Multi_HashMap_Malloc::MainKey::next()
    {
        _iHead = getHeadPtr()->_iNext;
        _pHead = _pMap->getMainKeyAbsolute(_iHead);

        return _iHead != 0;
    }

    bool TC_Multi_HashMap_Malloc::MainKey::prev()
    {
        _iHead = getHeadPtr()->_iPrev;
        _pHead = _pMap->getMainKeyAbsolute(_iHead);

        return _iHead != 0;
    }

    void TC_Multi_HashMap_Malloc::MainKey::deallocate()
    {
        if (HASNEXTCHUNK())
        {
            // 释放所有chunk
            deallocate(getHeadPtr()->_iNextChunk);
        }

        MemChunk chunk;
        chunk._iAddr = _iHead;
        chunk._iSize = getHeadPtr()->_iSize;
        _pMap->_pMainKeyAllocator->deallocateMemChunk(chunk, true);
    }

    void TC_Multi_HashMap_Malloc::MainKey::deallocate(uint32_t iChunk)
    {
        vector<MemChunk> v;
        MemChunk chunk;

        tagChunkHead *pChunk = getChunkHead(iChunk);
        chunk._iAddr = iChunk;
        chunk._iSize = pChunk->_iSize;
        v.push_back(chunk);

        //获取所有后续的chunk地址
        while (true)
        {
            if (pChunk->_bNextChunk)
            {
                chunk._iAddr = pChunk->_iNextChunk;
                pChunk = getChunkHead(pChunk->_iNextChunk);
                chunk._iSize = pChunk->_iSize;
                v.push_back(chunk);
            }
            else
            {
                break;
            }
        }

        // 空间全部释放掉
        _pMap->_pMainKeyAllocator->deallocateMemChunk(v, true);
    }

    void TC_Multi_HashMap_Malloc::MainKey::makeNew(uint32_t iIndex)
    {
        //getHeadPtr()->_iSize		= iAllocSize;
        getHeadPtr()->_iIndex = iIndex;
        getHeadPtr()->_iBlockHead = 0;
        getHeadPtr()->_iBlockTail = 0;
        getHeadPtr()->_iNext = 0;
        getHeadPtr()->_iPrev = 0;
        getHeadPtr()->_iGetNext = 0;
        getHeadPtr()->_iGetPrev = 0;
        getHeadPtr()->_iBlockCount = 0;
        getHeadPtr()->_iDataLen = 0;
        getHeadPtr()->_iBitset = 0;
        SETNEXTCHUNK(false);
        SETFULLDATA(false);

        _pMap->incMainKeyCount();
        _pMap->incMainKeyListCount(iIndex);
        _pMap->incOnlyKeyCountM();		// 缺省认为only key

        // 挂到主key链上
        if (_pMap->itemMainKey(iIndex)->_iMainKeyAddr == 0)
        {
            //当前hash桶没有元素
            _pMap->saveValue(&_pMap->itemMainKey(iIndex)->_iMainKeyAddr, _iHead);
            _pMap->saveValue(&getHeadPtr()->_iNext, (uint32_t)0);
            _pMap->saveValue(&getHeadPtr()->_iPrev, (uint32_t)0);
        }
        else
        {
            //当前hash桶有元素, 挂在桶开头
            _pMap->saveValue(&getHeadPtr(_pMap->itemMainKey(iIndex)->_iMainKeyAddr)->_iPrev, _iHead);
            _pMap->saveValue(&getHeadPtr()->_iNext, _pMap->itemMainKey(iIndex)->_iMainKeyAddr);
            _pMap->saveValue(&_pMap->itemMainKey(iIndex)->_iMainKeyAddr, _iHead);
            _pMap->saveValue(&getHeadPtr()->_iPrev, (uint32_t)0);
        }

        //挂在Get链表头部
        if (_pMap->_pHead->_iGetHead == 0)
        {
            assert(_pMap->_pHead->_iGetTail == 0);
            _pMap->saveValue(&_pMap->_pHead->_iGetHead, _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iGetTail, _iHead);
        }
        else
        {
            assert(_pMap->_pHead->_iGetTail != 0);
            _pMap->saveValue(&getHeadPtr()->_iGetNext, _pMap->_pHead->_iGetHead);
            _pMap->saveValue(&getHeadPtr(_pMap->_pHead->_iGetHead)->_iGetPrev, _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iGetHead, _iHead);
        }
    }

    int TC_Multi_HashMap_Malloc::MainKey::erase(vector<Value> &vtData)
    {
        ///////////////////删除主key下的所有block/////////////////////////////
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);
        string mk;
        int ret;
        if (keyType != MainKey::ZSET_TYPE)
            ret = get(mk);
        else
            ret = getData(mk);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        if (MainKey::ZSET_TYPE != keyType)
        {
            while (getHeadPtr()->_iBlockHead != 0)
            {
                Block block(_pMap, getHeadPtr()->_iBlockHead);
                BlockData data;
                int ret = TC_Multi_HashMap_Malloc::RT_OK;
                if (MainKey::HASH_TYPE == keyType)
                    ret = block.getBlockData(data);
                else if (MainKey::LIST_TYPE == keyType)
                    ret = block.getListBlockData(data);
                else if (MainKey::SET_TYPE == keyType)
                    ret = block.getSetBlockData(data);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    Value value(mk, data);
                    vtData.push_back(value);
                }

                if (MainKey::HASH_TYPE == keyType || MainKey::SET_TYPE == keyType)
                    block.erase(false);
                else if (MainKey::LIST_TYPE == keyType)
                    block.eraseList(false);

            }
        }
        else
        {
            while (getNext(0) != 0)
            {
                Block block(_pMap, getNext(0));
                BlockData data;
                int ret = block.getZSetBlockData(data);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    Value value(mk, data);
                    vtData.push_back(value);
                }

                //_pMap->delSkipList(*this, *Next(0));
                block.eraseZSet(false);
            }
        }

        // 切换当前使用的数据保护区，因为MainKey::erase操作内部会update
        _pMap->_pstCurrModify = _pMap->_pstInnerModify;

        //////////////////修改备份数据链表/////////////
        if (_pMap->_pHead->_iBackupTail == _iHead)
        {
            _pMap->saveValue(&_pMap->_pHead->_iBackupTail, getHeadPtr()->_iGetPrev);
        }

        ////////////////////修改Get链表的数据//////////
        //
        {
            bool bHead = (_pMap->_pHead->_iGetHead == _iHead);
            bool bTail = (_pMap->_pHead->_iGetTail == _iHead);

            if (!bHead)
            {
                assert(getHeadPtr()->_iGetPrev != 0);
                if (bTail)
                {
                    assert(getHeadPtr()->_iGetNext == 0);
                    //是尾部, 尾部指针指向上一个元素
                    _pMap->saveValue(&_pMap->_pHead->_iGetTail, getHeadPtr()->_iGetPrev);
                    _pMap->saveValue(&getHeadPtr(getHeadPtr()->_iGetPrev)->_iGetNext, (uint32_t)0);
                }
                else
                {
                    //不是头部也不是尾部
                    assert(getHeadPtr()->_iGetNext != 0);
                    _pMap->saveValue(&getHeadPtr(getHeadPtr()->_iGetPrev)->_iGetNext, getHeadPtr()->_iGetNext);
                    _pMap->saveValue(&getHeadPtr(getHeadPtr()->_iGetNext)->_iGetPrev, getHeadPtr()->_iGetPrev);
                }

                //修改统计指针
                if (_pMap->_iReadP == _iHead)
                    _pMap->_iReadP = getHeadPtr()->_iGetPrev;
                if (_pMap->_iReadPBak == _iHead)
                    _pMap->_iReadPBak = getHeadPtr()->_iGetPrev;
            }
            else
            {
                assert(getHeadPtr()->_iGetPrev == 0);
                if (bTail)
                {
                    assert(getHeadPtr()->_iGetNext == 0);
                    //头部也是尾部, 指针都设置为0
                    _pMap->saveValue(&_pMap->_pHead->_iGetHead, (uint32_t)0);
                    _pMap->saveValue(&_pMap->_pHead->_iGetTail, (uint32_t)0);
                }
                else
                {
                    //头部不是尾部, 头部指针指向下一个元素
                    assert(getHeadPtr()->_iGetNext != 0);
                    _pMap->saveValue(&_pMap->_pHead->_iGetHead, getHeadPtr()->_iGetNext);
                    //下一个元素上指针为0
                    _pMap->saveValue(&getHeadPtr(getHeadPtr()->_iGetNext)->_iGetPrev, (uint32_t)0);
                }

                //修改统计指针
                if (_pMap->_iReadP == _iHead)
                    _pMap->_iReadP = getHeadPtr()->_iGetNext;
                if (_pMap->_iReadPBak == _iHead)
                    _pMap->_iReadPBak = getHeadPtr()->_iGetNext;
            }
        }

        ///////////////////从主key链表中去掉///////////
        //
        //上一个主key指向下一个主key
        if (getHeadPtr()->_iPrev != 0)
        {
            _pMap->saveValue(&getHeadPtr(getHeadPtr()->_iPrev)->_iNext, getHeadPtr()->_iNext);
        }

        //下一个主key指向上一个
        if (getHeadPtr()->_iNext != 0)
        {
            _pMap->saveValue(&getHeadPtr(getHeadPtr()->_iNext)->_iPrev, getHeadPtr()->_iPrev);
        }

        //////////////////如果是hash头部, 需要修改hash索引数据指针//////
        //
        _pMap->delMainKeyListCount(getHeadPtr()->_iIndex);
        if (getHeadPtr()->_iPrev == 0)
        {
            //如果是hash桶的头部, 则还需要处理
            TC_Multi_HashMap_Malloc::tagMainKeyHashItem *pItem = _pMap->itemMainKey(getHeadPtr()->_iIndex);
            assert(pItem->_iMainKeyAddr == _iHead);
            if (pItem->_iMainKeyAddr == _iHead)
            {
                _pMap->saveValue(&pItem->_iMainKeyAddr, getHeadPtr()->_iNext);
            }
        }

        _pMap->delMainKeyCount();
        _pMap->delOnlyKeyCountM(); // 找个后续还要优化，不能直接减吧

        // 先保存block的地址，成功后即使core掉，也会在重启恢复时erase掉
        _pMap->saveAddr(_iHead, -2, true);

        // 确认之前的操作，保证MainKey会被erase
        _pMap->doUpdate4();

        _pMap->_pstCurrModify = _pMap->_pstOuterModify;

        return RT_OK;
    }

    void TC_Multi_HashMap_Malloc::MainKey::refreshGetList()
    {
        assert(_pMap->_pHead->_iGetHead != 0);
        assert(_pMap->_pHead->_iGetTail != 0);

        // 本身已经在头部，不需要处理
        if (_pMap->_pHead->_iGetHead == _iHead)
        {
            return;
        }

        uint32_t iPrev = getHeadPtr()->_iGetPrev;
        uint32_t iNext = getHeadPtr()->_iGetNext;

        assert(iPrev != 0);

        //是正在备份的数据
        if (_pMap->_pHead->_iBackupTail == _iHead)
        {
            _pMap->saveValue(&_pMap->_pHead->_iBackupTail, iPrev);
        }

        //更新统计指针
        if (_pMap->_iReadP == _iHead)
        {
            _pMap->_iReadP = iPrev;
        }
        if (_pMap->_iReadPBak == _iHead)
        {
            _pMap->_iReadPBak = iPrev;
        }

        //挂在链表头部
        _pMap->saveValue(&getHeadPtr()->_iGetNext, _pMap->_pHead->_iGetHead);
        _pMap->saveValue(&getHeadPtr(_pMap->_pHead->_iGetHead)->_iGetPrev, _iHead);
        _pMap->saveValue(&_pMap->_pHead->_iGetHead, _iHead);
        _pMap->saveValue(&getHeadPtr()->_iGetPrev, (uint32_t)0);

        //上一个元素的Next指针指向下一个元素
        _pMap->saveValue(&getHeadPtr(iPrev)->_iGetNext, iNext);

        //下一个元素的Prev指向上一个元素
        if (iNext != 0)
        {
            _pMap->saveValue(&getHeadPtr(iNext)->_iGetPrev, iPrev);
        }
        else
        {
            //改变尾部指针
            assert(_pMap->_pHead->_iGetTail == _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iGetTail, iPrev);
        }
    }

    int TC_Multi_HashMap_Malloc::MainKey::allocate(uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        uint32_t fn = 0;
        //一个块的真正的数据容量
        fn = getHeadPtr()->_iSize - sizeof(tagMainKeyHead);
        if (fn >= iDataLen)
        {
            //一个chunk就可以了, 后续的chunk都要释放掉
            if (HASNEXTCHUNK())
            {
                uint32_t iNextChunk = getHeadPtr()->_iNextChunk;

                //先保存第一个chunk的地址
                _pMap->saveAddr(iNextChunk, -1, true);
                //然后修改自己的区块
                _pMap->saveValue(&getHeadPtr()->_iBitset, NEXTCHUNK_BIT, false);
                _pMap->saveValue(&getHeadPtr()->_iDataLen, (uint32_t)0);
                //修改成功后再释放区块, 从而保证, 不会core的时候导致整个内存块不可用
                _pMap->doUpdate3();
            }
            return TC_Multi_HashMap_Malloc::RT_OK;
        }

        //计算还需要多少长度
        fn = iDataLen - fn;

        if (HASNEXTCHUNK())
        {
            tagChunkHead *pChunk = getChunkHead(getHeadPtr()->_iNextChunk);

            while (true)
            {
                if (fn <= (pChunk->_iSize - sizeof(tagChunkHead)))
                {
                    //已经不需要chunk了, 释放多余的chunk
                    if (pChunk->_bNextChunk)
                    {
                        uint32_t iNextChunk = pChunk->_iNextChunk;

                        //先保存第一个chunk的地址
                        _pMap->saveAddr(iNextChunk, -1, true);
                        //然后修改自己的区块
                        _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)false);
                        _pMap->saveValue(&pChunk->_iDataLen, (uint32_t)0);
                        //修改成功后再释放区块，从而保证出现core时不会出现整个内存块不可用
                        _pMap->doUpdate3();

                    }
                    return TC_Multi_HashMap_Malloc::RT_OK;
                }

                //计算, 还需要多少长度
                fn -= pChunk->_iSize - sizeof(tagChunkHead);

                if (pChunk->_bNextChunk)
                {
                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
                else
                {
                    //没有后续chunk了, 需要新分配fn的空间
                    vector<uint32_t> chunks;
                    int ret = allocateChunk(fn, chunks, vtData);
                    if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //没有内存可以分配
                        return ret;
                    }

                    _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)true);
                    _pMap->saveValue(&pChunk->_iNextChunk, chunks[0]);
                    //chunk指向分配的第一个chunk
                    pChunk = getChunkHead(chunks[0]);

                    pChunk->_bNextChunk = false;
                    pChunk->_iDataLen = (uint32_t)0;
                    _pMap->saveAddr(chunks[0], 0, true);

                    //连接每个chunk
                    return joinChunk(pChunk, chunks);
                }
            }
        }
        else
        {
            //没有后续chunk了, 需要新分配fn空间
            vector<uint32_t> chunks;
            int ret = allocateChunk(fn, chunks, vtData);
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                //没有内存可以分配
                return ret;
            }

            _pMap->saveValue(&getHeadPtr()->_iBitset, NEXTCHUNK_BIT, true);
            _pMap->saveValue(&getHeadPtr()->_iNextChunk, chunks[0]);
            //chunk指向分配的第一个chunk
            tagChunkHead *pChunk = getChunkHead(chunks[0]);

            pChunk->_bNextChunk = false;
            pChunk->_iDataLen = (uint32_t)0;
            _pMap->saveAddr(chunks[0], 0, true);

            //连接每个chunk
            return joinChunk(pChunk, chunks);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::MainKey::joinChunk(tagChunkHead *pChunk, const vector<uint32_t>& chunks)
    {
        tagChunkHead* pNextChunk = NULL;
        for (size_t i = 0; i < chunks.size(); ++i)
        {
            if (i == chunks.size() - 1)
            {
                _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)false);
            }
            else
            {
                pNextChunk = getChunkHead(chunks[i + 1]);
                pNextChunk->_bNextChunk = false;
                pNextChunk->_iDataLen = (uint32_t)0;

                _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)true);
                _pMap->saveValue(&pChunk->_iNextChunk, chunks[i + 1]);
                if (i > 0 && i % 10 == 0)
                {
                    _pMap->doUpdate2();
                }

                pChunk = pNextChunk;
            }
        }

        _pMap->doUpdate2();

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::MainKey::allocateChunk(uint32_t fn, vector<uint32_t> &chunks, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        assert(fn > 0);

        while (true)
        {
            uint32_t iAllocSize = fn + sizeof(tagChunkHead);
            // 分配空间
            // 分配过程中可能会淘汰数据，第一个参数是当前的主key头地址，不能被淘汰
            uint32_t t = _pMap->_pMainKeyAllocator->allocateMainKeyChunk(_iHead, iAllocSize, vtData);
            if (t == 0)
            {
                //没有内存可以分配了, 先把分配的归还
                _pMap->_pMainKeyAllocator->deallocateMainKeyChunk(chunks);
                chunks.clear();
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }

            //设置分配的数据块的大小
            getChunkHead(t)->_iSize = iAllocSize;

            chunks.push_back(t);

            //分配够了
            if (fn <= (iAllocSize - sizeof(tagChunkHead)))
            {
                break;
            }

            //还需要多少空间
            fn -= iAllocSize - sizeof(tagChunkHead);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    uint32_t TC_Multi_HashMap_Malloc::MainKey::getDataLen()
    {
        uint32_t n = 0;
        if (!HASNEXTCHUNK())
        {
            // 只有一个chunk
            n += getHeadPtr()->_iDataLen;
            return n;
        }

        //当前块的大小
        n += getHeadPtr()->_iSize - sizeof(tagMainKeyHead);
        tagChunkHead *pChunk = getChunkHead(getHeadPtr()->_iNextChunk);

        while (true)
        {
            if (pChunk->_bNextChunk)
            {
                //当前块的大小
                n += pChunk->_iSize - sizeof(tagChunkHead);
                pChunk = getChunkHead(pChunk->_iNextChunk);
            }
            else
            {
                // 最后一个chunk
                n += pChunk->_iDataLen;
                break;
            }
        }

        return n;
    }

    int TC_Multi_HashMap_Malloc::MainKey::get(void *pData, uint32_t &iDataLen)
    {
        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            iDataLen = min(getHeadPtr()->_iDataLen, iDataLen);
            memcpy(pData, getHeadPtr()->_cData, iDataLen);
            return TC_Multi_HashMap_Malloc::RT_OK;
        }
        else
        {
            uint32_t iUseSize = getHeadPtr()->_iSize - sizeof(tagMainKeyHead);
            uint32_t iCopyLen = min(iUseSize, iDataLen);

            // 先copy第一个chunk的数据
            memcpy(pData, getHeadPtr()->_cData, iCopyLen);
            if (iDataLen < iUseSize)
            {
                return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;   // copy数据不完全
            }

            // 已经copy长度
            uint32_t iHasLen = iCopyLen;
            // 最大剩余长度
            uint32_t iLeftLen = iDataLen - iCopyLen;


            // copy后面所有的chunk中的数据
            tagChunkHead *pChunk = getChunkHead(getHeadPtr()->_iNextChunk);
            while (iHasLen < iDataLen)
            {
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);
                if (!pChunk->_bNextChunk)
                {
                    uint32_t iCopyLen = min(pChunk->_iDataLen, iLeftLen);
                    memcpy((char*)pData + iHasLen, pChunk->_cData, iCopyLen);
                    iDataLen = iHasLen + iCopyLen;

                    if (iLeftLen < pChunk->_iDataLen)
                    {
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;       // copy不完全
                    }

                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {
                    uint32_t iCopyLen = min(iUseSize, iLeftLen);
                    memcpy((char*)pData + iHasLen, pChunk->_cData, iCopyLen);
                    if (iLeftLen <= iUseSize)
                    {
                        iDataLen = iHasLen + iCopyLen;
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;   // copy不完全
                    }

                    iHasLen += iCopyLen;
                    iLeftLen -= iCopyLen;

                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::MainKey::get(string &mk)
    {
        // 获取数据长度
        uint32_t iLen = getDataLen();

        if (iLen > _pMap->_tmpBufSize)
        {
            delete[] _pMap->_tmpBuf;
            _pMap->_tmpBufSize = iLen;
            _pMap->_tmpBuf = new char[_pMap->_tmpBufSize];
        }
        uint32_t iGetLen = iLen;
        int ret = get(_pMap->_tmpBuf, iGetLen);
        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
        {
            // 解码成string
            mk.assign(_pMap->_tmpBuf, iGetLen);
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::MainKey::set(const void *pData, uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        // 首先分配刚刚够的长度, 不能多一个chunk, 也不能少一个chunk
        // allocate会判断当前chunk的长度是否满足iDataLen，少了就加chunk，多了就释放chunk
        int ret = allocate(iDataLen, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 第一个chunk的有效长度
        uint32_t iUseSize = getHeadPtr()->_iSize - sizeof(tagMainKeyHead);
        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            // 先copy数据, 再复制数据长度
            memcpy(getHeadPtr()->_cData, (char*)pData, iDataLen);
            getHeadPtr()->_iDataLen = iDataLen;
        }
        else
        {
            // copy到当前的chunk中
            memcpy(getHeadPtr()->_cData, (char*)pData, iUseSize);
            // 剩余程度
            uint32_t iLeftLen = iDataLen - iUseSize;
            uint32_t iCopyLen = iUseSize;

            tagChunkHead *pChunk = getChunkHead(getHeadPtr()->_iNextChunk);
            while (true)
            {
                // 计算chunk的可用大小
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);

                if (!pChunk->_bNextChunk)
                {
                    assert(iUseSize >= iLeftLen);
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iLeftLen);
                    // 最后一个chunk, 才有数据长度, 先copy数据再赋值长度
                    pChunk->_iDataLen = iLeftLen;
                    iCopyLen += iLeftLen;
                    iLeftLen -= iLeftLen;
                    break;
                }
                else
                {
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iUseSize);
                    iCopyLen += iUseSize;
                    iLeftLen -= iUseSize;

                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
            assert(iLeftLen == 0);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::getBlockData(TC_Multi_HashMap_Malloc::BlockData &data)
    {
        data._isDelete = isDelete() ? DELETE_TRUE : DELETE_FALSE;
        data._dirty = isDirty();
        data._synct = getSyncTime();
        data._iVersion = getVersion();
        data._expire = getExpireTime();
        data._keyType = MainKey::HASH_TYPE;

        string s;
        int ret = get(s);

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        try
        {
            // block保存的数据中，第一部分为除主key外的联合主键
            TC_PackOut po(s.c_str(), s.length());
            po >> data._key;

            // 如果不是只有Key
            if (!isOnlyKey())
            {
                // 第二部分为数据值
                po >> data._value;
            }
            else
            {
                return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
            }
        }
        catch (exception &ex)
        {
            ret = TC_Multi_HashMap_Malloc::RT_DECODE_ERR;
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::Block::getListBlockData(TC_Multi_HashMap_Malloc::BlockData &data)
    {
        data._isDelete = DELETE_FALSE;
        data._dirty = false;
        data._synct = 0;
        data._iVersion = 0;
        data._expire = getListBlockHead()->_iExpireTime;
        data._keyType = MainKey::LIST_TYPE;

        return getList(data._value);
    }

    int TC_Multi_HashMap_Malloc::Block::getSetBlockData(TC_Multi_HashMap_Malloc::BlockData &data)
    {
        data._isDelete = isDelete() ? DELETE_TRUE : DELETE_FALSE;
        data._dirty = isDirty();
        data._synct = getSyncTime();
        data._iVersion = getVersion();
        data._expire = getExpireTime();
        data._keyType = MainKey::SET_TYPE;

        return get(data._value);

    }

    int TC_Multi_HashMap_Malloc::Block::getZSetBlockData(TC_Multi_HashMap_Malloc::BlockData &data)
    {
        data._isDelete = isDelete() ? DELETE_TRUE : DELETE_FALSE;
        data._dirty = isDirty();
        data._synct = getSyncTime();
        data._iVersion = getVersion();
        data._expire = getExpireTime();
        data._score = getScore();
        data._keyType = MainKey::ZSET_TYPE;

        int ret = getData(data._value);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::Block::getBlockData(TC_Multi_HashMap_Malloc::BlockPackData &data)
    {
        data._isDelete = isDelete() ? DELETE_TRUE : DELETE_FALSE;
        data._dirty = isDirty();
        data._synct = getSyncTime();
        data._iVersion = getVersion();
        data._expire = getExpireTime();

        string s;
        int ret = get(s);

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        data._keyValue = s;

        if (isOnlyKey())
        {
            return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::getDataByPos(char *ptr, int len, uint32_t pos)
    {
        assert(ptr != NULL);

        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            uint32_t iDataLen = getBlockHead()->_iDataLen;
            if ((pos + len) >= iDataLen)
                return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;

            memcpy(ptr, getBlockHead()->_cData + pos, len);
            return TC_Multi_HashMap_Malloc::RT_OK;
        }
        else
        {
            uint32_t iUseSize = getBlockHead()->_iSize - sizeof(tagBlockHead);

            if (pos < iUseSize)
            {
                if ((pos + len) <= iUseSize)
                {//全部都在这了，可以直接返回
                    memcpy(ptr, getBlockHead()->_cData + pos, len);
                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {//只有一部分
                    memcpy(ptr, getBlockHead()->_cData + pos, iUseSize - pos);
                    ptr += iUseSize - pos;
                    len -= iUseSize - pos;
                    pos = 0;
                }
            }
            else
            {//都不在这里
                pos = pos - iUseSize;
            }

            tagChunkHead *pChunk = getChunkHead(getBlockHead()->_iNextChunk);
            while (len > 0)
            {
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);

                if (pos < iUseSize)
                {
                    if ((pos + len) <= iUseSize)
                    {//全部都在这了，可以直接返回
                        memcpy(ptr, pChunk->_cData + pos, len);
                        return TC_Multi_HashMap_Malloc::RT_OK;
                    }
                    else
                    {//只有一部分
                        memcpy(ptr, pChunk->_cData + pos, iUseSize - pos);
                        ptr += iUseSize - pos;
                        len -= iUseSize - pos;
                        pos = 0;
                    }
                }
                else
                {
                    pos = pos - iUseSize;
                }

                if (!pChunk->_bNextChunk)
                {
                    //最后一个了，如果到这里数据还没有找到，那就有问题了
                    if (len < 0 || len > 0)
                    {
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;
                    }

                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {
                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::setDataByPos(uint32_t data, uint32_t pos)
    {
        char *ptr = (char*)&data;
        int len = 4;
        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            uint32_t iDataLen = getBlockHead()->_iDataLen;
            if ((pos + sizeof(uint32_t)) >= iDataLen)
                return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;

            _pMap->saveValue(getBlockHead()->_cData + pos, data);
            return TC_Multi_HashMap_Malloc::RT_OK;
        }
        else
        {
            uint32_t iUseSize = getBlockHead()->_iSize - sizeof(tagBlockHead);

            if (pos < iUseSize)
            {
                if ((pos + len) <= iUseSize)
                {//全部都在这了，可以直接返回
                    _pMap->saveValue(getBlockHead()->_cData + pos, data);
                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {//只有一部分
                    uint8_t i8Data;
                    uint16_t i16Data;
                    switch (iUseSize - pos)
                    {
                    case 1:
                        i8Data = *(uint8_t*)ptr;
                        _pMap->saveValue(getBlockHead()->_cData + pos, i8Data);
                        break;
                    case 2:
                        i16Data = *(uint16_t*)ptr;
                        _pMap->saveValue(getBlockHead()->_cData + pos, i16Data);
                        break;
                    case 3:
                        i16Data = *(uint16_t*)ptr;
                        _pMap->saveValue(getBlockHead()->_cData + pos, i16Data);
                        i8Data = *(uint8_t*)(ptr + sizeof(uint16_t));
                        _pMap->saveValue(getBlockHead()->_cData + pos + sizeof(uint16_t), i8Data);
                        break;
                    default:
                        assert(false);
                    }
                    ptr += iUseSize - pos;
                    len -= iUseSize - pos;
                    pos = 0;
                }
            }
            else
            {//都不在这里
                pos = pos - iUseSize;
            }

            tagChunkHead *pChunk = getChunkHead(getBlockHead()->_iNextChunk);
            while (len > 0)
            {
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);

                if (pos < iUseSize)
                {
                    uint8_t i8Data;
                    uint16_t i16Data;
                    uint32_t i32Data;
                    if ((pos + len) <= iUseSize)
                    {//全部都在这了，可以直接返回
                        switch (len)
                        {
                        case 1:
                            i8Data = *(uint8_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i8Data);
                            break;
                        case 2:
                            i16Data = *(uint16_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i16Data);
                            break;
                        case 3:
                            i16Data = *(uint16_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i16Data);
                            i8Data = *(uint8_t*)(ptr + sizeof(uint16_t));
                            _pMap->saveValue(pChunk->_cData + pos + sizeof(uint16_t), i8Data);
                            break;
                        case 4:
                            i32Data = *(uint32_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i32Data);
                            break;
                        default:
                            assert(false);
                        }
                        return TC_Multi_HashMap_Malloc::RT_OK;
                    }
                    else
                    {//只有一部分
                        switch (iUseSize - pos)
                        {
                        case 1:
                            i8Data = *(uint8_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i8Data);
                            break;
                        case 2:
                            i16Data = *(uint16_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i16Data);
                            break;
                        case 3:
                            i16Data = *(uint16_t*)ptr;
                            _pMap->saveValue(pChunk->_cData + pos, i16Data);
                            i8Data = *(uint8_t*)(ptr + sizeof(uint16_t));
                            _pMap->saveValue(pChunk->_cData + pos + sizeof(uint16_t), i8Data);
                            break;
                        default:
                            assert(false);
                        }
                        ptr += iUseSize - pos;
                        len -= iUseSize - pos;
                        pos = 0;
                    }
                }
                else
                {
                    pos = pos - iUseSize;
                }

                if (!pChunk->_bNextChunk)
                {
                    //最后一个了，如果到这里数据还没有找到，那就有问题了
                    if (len < 0 || len > 0)
                    {
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;
                    }

                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {
                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    uint32_t TC_Multi_HashMap_Malloc::Block::getLastBlockHead()
    {
        uint32_t iHead = _iHead;

        // 获取当前联合主键block链上的最后一个block
        while (getBlockHead(iHead)->_iUKBlockNext != 0)
        {
            iHead = getBlockHead(iHead)->_iUKBlockNext;
        }

        return iHead;
    }

    int TC_Multi_HashMap_Malloc::Block::get(void *pData, uint32_t &iDataLen)
    {
        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            memcpy(pData, getBlockHead()->_cData, min(getBlockHead()->_iDataLen, iDataLen));
            iDataLen = getBlockHead()->_iDataLen;
            return TC_Multi_HashMap_Malloc::RT_OK;
        }
        else
        {
            // 第一个chunk的有效空间长度
            uint32_t iUseSize = getBlockHead()->_iSize - sizeof(tagBlockHead);
            uint32_t iCopyLen = min(iUseSize, iDataLen);

            // copy第一个chunk中的数据
            memcpy(pData, getBlockHead()->_cData, iCopyLen);
            if (iDataLen < iUseSize)
            {
                // 外部提供的缓冲区长度不够
                return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;   //copy数据不完全
            }

            // 已经copy长度
            uint32_t iHasLen = iCopyLen;
            // 最大剩余长度
            uint32_t iLeftLen = iDataLen - iCopyLen;

            tagChunkHead *pChunk = getChunkHead(getBlockHead()->_iNextChunk);
            while (iHasLen < iDataLen)
            {
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);
                if (!pChunk->_bNextChunk)
                {
                    // 最后一个chunk
                    uint32_t iCopyLen = min(pChunk->_iDataLen, iLeftLen);
                    memcpy((char*)pData + iHasLen, pChunk->_cData, iCopyLen);
                    iDataLen = iHasLen + iCopyLen;

                    if (iLeftLen < pChunk->_iDataLen)
                    {
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;       // copy不完全
                    }

                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {
                    uint32_t iCopyLen = min(iUseSize, iLeftLen);
                    // copy当前的chunk
                    memcpy((char*)pData + iHasLen, pChunk->_cData, iCopyLen);
                    if (iLeftLen <= iUseSize)
                    {
                        iDataLen = iHasLen + iCopyLen;
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;   // copy不完全
                    }

                    // copy当前chunk完全
                    iHasLen += iCopyLen;
                    iLeftLen -= iCopyLen;

                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::getList(void *pData, uint32_t &iDataLen)
    {
        if (!HASNEXTCHUNKLIST())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            memcpy(pData, getListBlockHead()->_cData, min(getListBlockHead()->_iDataLen, iDataLen));
            iDataLen = getListBlockHead()->_iDataLen;
            return TC_Multi_HashMap_Malloc::RT_OK;
        }
        else
        {
            // 第一个chunk的有效空间长度
            uint32_t iUseSize = getListBlockHead()->_iSize - sizeof(tagListBlockHead);
            uint32_t iCopyLen = min(iUseSize, iDataLen);

            // copy第一个chunk中的数据
            memcpy(pData, getListBlockHead()->_cData, iCopyLen);
            if (iDataLen < iUseSize)
            {
                // 外部提供的缓冲区长度不够
                return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;   //copy数据不完全
            }

            // 已经copy长度
            uint32_t iHasLen = iCopyLen;
            // 最大剩余长度
            uint32_t iLeftLen = iDataLen - iCopyLen;

            tagChunkHead *pChunk = getChunkHead(getListBlockHead()->_iNextChunk);
            while (iHasLen < iDataLen)
            {
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);
                if (!pChunk->_bNextChunk)
                {
                    // 最后一个chunk
                    uint32_t iCopyLen = min(pChunk->_iDataLen, iLeftLen);
                    memcpy((char*)pData + iHasLen, pChunk->_cData, iCopyLen);
                    iDataLen = iHasLen + iCopyLen;

                    if (iLeftLen < pChunk->_iDataLen)
                    {
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;       // copy不完全
                    }

                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else
                {
                    uint32_t iCopyLen = min(iUseSize, iLeftLen);
                    // copy当前的chunk
                    memcpy((char*)pData + iHasLen, pChunk->_cData, iCopyLen);
                    if (iLeftLen <= iUseSize)
                    {
                        iDataLen = iHasLen + iCopyLen;
                        return TC_Multi_HashMap_Malloc::RT_NOTALL_ERR;   // copy不完全
                    }

                    // copy当前chunk完全
                    iHasLen += iCopyLen;
                    iLeftLen -= iCopyLen;

                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::get(string &s)
    {
        uint32_t iLen = getDataLen();

        if (iLen > _pMap->_tmpBufSize)
        {
            delete[] _pMap->_tmpBuf;
            _pMap->_tmpBufSize = iLen;
            _pMap->_tmpBuf = new char[_pMap->_tmpBufSize];
        }
        uint32_t iGetLen = iLen;
        int ret = get(_pMap->_tmpBuf, iGetLen);
        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
        {
            s.assign(_pMap->_tmpBuf, iGetLen);
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::Block::getList(string &s)
    {
        uint32_t iLen = getListDataLen();

        if (iLen > _pMap->_tmpBufSize)
        {
            delete[] _pMap->_tmpBuf;
            _pMap->_tmpBufSize = iLen;
            _pMap->_tmpBuf = new char[_pMap->_tmpBufSize];
        }
        uint32_t iGetLen = iLen;
        int ret = getList(_pMap->_tmpBuf, iGetLen);
        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
        {
            s.assign(_pMap->_tmpBuf, iGetLen);
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::Block::set(const void *pData, uint32_t iDataLen, bool bOnlyKey,
        uint32_t iExpireTime, uint8_t iVersion, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        // version为0表示外部不关心数据版本，onlykey时也不需要关注数据版本
        if (iVersion != 0 && !isOnlyKey() && getBlockHead()->_iVersion != iVersion)
        {
            // 数据版本不匹配
            return TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH;
        }

        // 首先分配刚刚够的长度, 不能多一个chunk, 也不能少一个chunk
        // allocate会判断当前chunk的长度是否满足iDataLen，少了就加chunk，多了就释放chunk
        int ret = allocate(iDataLen, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        if (bOnlyKey)
        {
            // 原始数据是脏数据
            if (isDirty())
            {
                _pMap->delDirtyCount();
            }

            // 数据被修改, 设置为脏数据
            SETDIRTY(false);

            // 原始数据不是OnlyKey数据
            if (!isOnlyKey())
            {
                _pMap->incOnlyKeyCount();
            }

            // 清空过期时间
            setExpireTime(0);
        }
        else
        {
            //原始数据不是脏数据
            if (!isDirty())
            {
                _pMap->incDirtyCount();
            }

            //数据被修改, 设置为脏数据
            SETDIRTY(true);

            //原始数据是OnlyKey数据
            if (isOnlyKey())
            {
                _pMap->delOnlyKeyCount();
            }

            // Changed by tutuli 2017-2-27 15:06
            register time_t	lExpireTime = getBlockHead()->_iExpireTime;
            // 设置过期时间
            if (iExpireTime != 0)
            {
                setExpireTime(iExpireTime);
            }
            // Changed by tutuli 2017-2-27 15:06
            // 当数据过期后，再调用set不带过期时间的接口时，将过期时间置为0
            else if ((iExpireTime == 0) && (lExpireTime != 0) && (lExpireTime < TC_TimeProvider::getInstance()->getNow())) {
                getBlockHead()->_iExpireTime = 0;
            }

            // 递增数据版本
            iVersion = getBlockHead()->_iVersion;
            iVersion++;
            if (iVersion == 0)
            {
                // 0是保留版本，有效版本范围是2-255
                //版本号1为初始版本号，通过这个版本号
                //用于实现有数据不能插入，没数据插入的功能
                iVersion = 2;
            }

            getBlockHead()->_iVersion = iVersion;
        }

        //设置是否只有Key
        SETONLYKEY(bOnlyKey);

        // 第一个chunk的有效空间大小
        uint32_t iUseSize = getBlockHead()->_iSize - sizeof(tagBlockHead);
        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            memcpy(getBlockHead()->_cData, (char*)pData, iDataLen);
            getBlockHead()->_iDataLen = iDataLen;
        }
        else
        {
            // copy到第一个chunk中
            memcpy(getBlockHead()->_cData, (char*)pData, iUseSize);
            // 剩余程度
            uint32_t iLeftLen = iDataLen - iUseSize;
            uint32_t iCopyLen = iUseSize;

            tagChunkHead *pChunk = getChunkHead(getBlockHead()->_iNextChunk);
            while (true)
            {
                // 计算chunk的可用大小
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);

                if (!pChunk->_bNextChunk)
                {
                    assert(iUseSize >= iLeftLen);
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iLeftLen);
                    // 最后一个chunk, 才有数据长度, 先copy数据再赋值长度
                    pChunk->_iDataLen = iLeftLen;
                    iCopyLen += iLeftLen;
                    iLeftLen -= iLeftLen;
                    break;
                }
                else
                {
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iUseSize);
                    iCopyLen += iUseSize;
                    iLeftLen -= iUseSize;

                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
            assert(iLeftLen == 0);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::set(const void *pData, uint32_t iDataLen, bool bOnlyKey,
        uint32_t iExpireTime, uint8_t iVersion, bool bCheckExpire, uint32_t iNowTime, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        //开启过期且数据已过期，或被置删除标志，则先将版本号设置为初始版本1.
        register uint32_t lExpireTime = getBlockHead()->_iExpireTime;
        if ((bCheckExpire && (lExpireTime != 0) && (lExpireTime < iNowTime)) || isDelete())
        {
            getBlockHead()->_iVersion = 1;
        }

        // version为0表示外部不关心数据版本，onlykey时也不需要关注数据版本
        if (iVersion != 0 && !isOnlyKey() && getBlockHead()->_iVersion != iVersion)
        {
            // 数据版本不匹配
            return TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH;
        }

        // 首先分配刚刚够的长度, 不能多一个chunk, 也不能少一个chunk
        // allocate会判断当前chunk的长度是否满足iDataLen，少了就加chunk，多了就释放chunk
        int ret = allocate(iDataLen, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        if (bOnlyKey)
        {
            // 原始数据是脏数据
            if (isDirty())
            {
                _pMap->delDirtyCount();
            }

            // 数据被修改, 设置为脏数据
            SETDIRTY(false);

            // 原始数据不是OnlyKey数据
            if (!isOnlyKey())
            {
                _pMap->incOnlyKeyCount();
            }

            // 清空过期时间
            setExpireTime(0);
        }
        else
        {
            //原始数据不是脏数据
            if (!isDirty())
            {
                _pMap->incDirtyCount();
            }

            //数据被修改, 设置为脏数据
            SETDIRTY(true);

            //原始数据是OnlyKey数据
            if (isOnlyKey())
            {
                _pMap->delOnlyKeyCount();
            }

            // 设置过期时间
            if (iExpireTime != 0)
            {
                setExpireTime(iExpireTime);
            }
            // Changed by tutuli 2017-2-27 15:06
            // 当数据过期后，再调用set不带过期时间的接口时，将过期时间置为0
            else if ((iExpireTime == 0) && (lExpireTime != 0) && (lExpireTime < (uint32_t)TC_TimeProvider::getInstance()->getNow())) {
                getBlockHead()->_iExpireTime = 0;
            }

            // 递增数据版本
            iVersion = getBlockHead()->_iVersion;
            iVersion++;
            if (iVersion == 0)
            {
                // 0是保留版本，有效版本范围是2-255
                //版本号1为初始版本号，通过这个版本号
                //用于实现有数据不能插入，没数据插入的功能
                iVersion = 2;
            }

            getBlockHead()->_iVersion = iVersion;
        }

        //设置是否只有Key
        SETONLYKEY(bOnlyKey);

        // 第一个chunk的有效空间大小
        uint32_t iUseSize = getBlockHead()->_iSize - sizeof(tagBlockHead);
        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            memcpy(getBlockHead()->_cData, (char*)pData, iDataLen);
            getBlockHead()->_iDataLen = iDataLen;
        }
        else
        {
            // copy到第一个chunk中
            memcpy(getBlockHead()->_cData, (char*)pData, iUseSize);
            // 剩余程度
            uint32_t iLeftLen = iDataLen - iUseSize;
            uint32_t iCopyLen = iUseSize;

            tagChunkHead *pChunk = getChunkHead(getBlockHead()->_iNextChunk);
            while (true)
            {
                // 计算chunk的可用大小
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);

                if (!pChunk->_bNextChunk)
                {
                    assert(iUseSize >= iLeftLen);
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iLeftLen);
                    // 最后一个chunk, 才有数据长度, 先copy数据再赋值长度
                    pChunk->_iDataLen = iLeftLen;
                    iCopyLen += iLeftLen;
                    iLeftLen -= iLeftLen;
                    break;
                }
                else
                {
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iUseSize);
                    iCopyLen += iUseSize;
                    iLeftLen -= iUseSize;

                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
            assert(iLeftLen == 0);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::set(const void *pData, uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        // 首先分配刚刚够的长度, 不能多一个chunk, 也不能少一个chunk
        // allocate会判断当前chunk的长度是否满足iDataLen，少了就加chunk，多了就释放chunk
        int ret = allocate(iDataLen, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 第一个chunk的有效空间大小
        uint32_t iUseSize = getBlockHead()->_iSize - sizeof(tagBlockHead);
        if (!HASNEXTCHUNK())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            memcpy(getBlockHead()->_cData, (char*)pData, iDataLen);
            getBlockHead()->_iDataLen = iDataLen;
        }
        else
        {
            // copy到第一个chunk中
            memcpy(getBlockHead()->_cData, (char*)pData, iUseSize);
            // 剩余程度
            uint32_t iLeftLen = iDataLen - iUseSize;
            uint32_t iCopyLen = iUseSize;

            tagChunkHead *pChunk = getChunkHead(getBlockHead()->_iNextChunk);
            while (true)
            {
                // 计算chunk的可用大小
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);

                if (!pChunk->_bNextChunk)
                {
                    assert(iUseSize >= iLeftLen);
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iLeftLen);
                    // 最后一个chunk, 才有数据长度, 先copy数据再赋值长度
                    pChunk->_iDataLen = iLeftLen;
                    iCopyLen += iLeftLen;
                    iLeftLen -= iLeftLen;
                    break;
                }
                else
                {
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iUseSize);
                    iCopyLen += iUseSize;
                    iLeftLen -= iUseSize;

                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
            assert(iLeftLen == 0);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::setList(const void *pData, uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        // 首先分配刚刚够的长度, 不能多一个chunk, 也不能少一个chunk
        // allocate会判断当前chunk的长度是否满足iDataLen，少了就加chunk，多了就释放chunk
        int ret = allocateList(iDataLen, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 第一个chunk的有效空间大小
        uint32_t iUseSize = getListBlockHead()->_iSize - sizeof(tagListBlockHead);
        if (!HASNEXTCHUNKLIST())
        {
            // 没有下一个chunk, 一个chunk就可以装下数据了
            memcpy(getListBlockHead()->_cData, (char*)pData, iDataLen);
            getListBlockHead()->_iDataLen = iDataLen;
        }
        else
        {
            // copy到第一个chunk中
            memcpy(getListBlockHead()->_cData, (char*)pData, iUseSize);
            // 剩余程度
            uint32_t iLeftLen = iDataLen - iUseSize;
            uint32_t iCopyLen = iUseSize;

            tagChunkHead *pChunk = getChunkHead(getListBlockHead()->_iNextChunk);
            while (true)
            {
                // 计算chunk的可用大小
                iUseSize = pChunk->_iSize - sizeof(tagChunkHead);

                if (!pChunk->_bNextChunk)
                {
                    assert(iUseSize >= iLeftLen);
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iLeftLen);
                    // 最后一个chunk, 才有数据长度, 先copy数据再赋值长度
                    pChunk->_iDataLen = iLeftLen;
                    iCopyLen += iLeftLen;
                    iLeftLen -= iLeftLen;
                    break;
                }
                else
                {
                    // copy到当前的chunk中
                    memcpy(pChunk->_cData, (char*)pData + iCopyLen, iUseSize);
                    iCopyLen += iUseSize;
                    iLeftLen -= iUseSize;

                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
            }
            assert(iLeftLen == 0);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    void TC_Multi_HashMap_Malloc::Block::setDirty(bool b)
    {
        if (b)
        {
            if (!isDirty())
            {
                SETDIRTY(b);
                _pMap->incDirtyCount();
            }
        }
        else
        {
            if (isDirty())
            {
                SETDIRTY(b);
                _pMap->delDirtyCount();
            }
        }
    }

    void TC_Multi_HashMap_Malloc::Block::setDelete(bool b)
    {
        if (b)
        {
            if (!isDelete())
            {
                SETDELETE(b);
                // 减少主key下数据个数
                MainKey mainKey(_pMap, getBlockHead()->_iMainKey);
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount - 1);
            }
        }
        else
        {
            if (isDelete())
            {
                SETDELETE(b);
                // 增加主key下数据个数
                MainKey mainKey(_pMap, getBlockHead()->_iMainKey);
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);
            }
        }
    }

    bool TC_Multi_HashMap_Malloc::Block::nextBlock()
    {
        _iHead = getBlockHead()->_iUKBlockNext;
        _pHead = _pMap->getAbsolute(_iHead);

        return _iHead != 0;
    }

    bool TC_Multi_HashMap_Malloc::Block::prevBlock()
    {
        _iHead = getBlockHead()->_iUKBlockPrev;
        _pHead = _pMap->getAbsolute(_iHead);

        return _iHead != 0;
    }

    void TC_Multi_HashMap_Malloc::Block::deallocate()
    {
        // 先释放所有的后续chunk
        if (HASNEXTCHUNK())
        {
            deallocate(getBlockHead()->_iNextChunk);
        }

        // 再释放第一个chunk
        MemChunk chunk;
        chunk._iAddr = _iHead;
        chunk._iSize = getBlockHead()->_iSize;
        _pMap->_pDataAllocator->deallocateMemChunk(chunk, false);
    }

    void TC_Multi_HashMap_Malloc::Block::makeNew(uint32_t iMainKeyAddr, uint32_t uIndex, bool bHead)
    {
        //getBlockHead()->_iSize          = iAllocSize;
        getBlockHead()->_iIndex = uIndex;
        getBlockHead()->_iUKBlockNext = 0;
        getBlockHead()->_iUKBlockPrev = 0;
        getBlockHead()->_iMKBlockNext = 0;
        getBlockHead()->_iMKBlockPrev = 0;
        getBlockHead()->_iSetNext = 0;
        getBlockHead()->_iSetPrev = 0;
        getBlockHead()->_iMainKey = iMainKeyAddr;
        getBlockHead()->_iSyncTime = 0;
        getBlockHead()->_iExpireTime = 0;
        getBlockHead()->_iDataLen = 0;
        getBlockHead()->_iVersion = 1;		// 有效版本范围1-255
        getBlockHead()->_iBitset = 0;        //把bitset设为0
        SETONLYKEY(false);
        SETDIRTY(true);
        SETNEXTCHUNK(false);
        SETDELETE(false);

        _pMap->incDirtyCount();
        _pMap->incElementCount();
        _pMap->incListCount(uIndex);

        // 增加主key下面的block数量
        MainKey mainKey(_pMap, iMainKeyAddr);
        _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);
        _pMap->updateMaxMainKeyBlockCount(mainKey.getHeadPtr()->_iBlockCount);

        // 挂在block链表上
        if (_pMap->item(uIndex)->_iBlockAddr == 0)
        {
            // 当前hash桶没有元素
            _pMap->saveValue(&_pMap->item(uIndex)->_iBlockAddr, _iHead);
            _pMap->saveValue(&getBlockHead()->_iUKBlockNext, (uint32_t)0);
            _pMap->saveValue(&getBlockHead()->_iUKBlockPrev, (uint32_t)0);
        }
        else
        {
            // 当前hash桶有元素, 挂在桶开头
            _pMap->saveValue(&getBlockHead(_pMap->item(uIndex)->_iBlockAddr)->_iUKBlockPrev, _iHead);
            _pMap->saveValue(&getBlockHead()->_iUKBlockNext, _pMap->item(uIndex)->_iBlockAddr);
            _pMap->saveValue(&_pMap->item(uIndex)->_iBlockAddr, _iHead);
            _pMap->saveValue(&getBlockHead()->_iUKBlockPrev, (uint32_t)0);
        }

        // 挂在主key链上
        if (mainKey.getHeadPtr()->_iBlockHead == 0)
        {
            // 当前主key链上还没有元素
            _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockHead, _iHead);
            _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockTail, _iHead);
            _pMap->saveValue(&getBlockHead()->_iMKBlockNext, (uint32_t)0);
            _pMap->saveValue(&getBlockHead()->_iMKBlockPrev, (uint32_t)0);

            // 减主key的only key计数
            _pMap->delOnlyKeyCountM();
        }
        else
        {
            // 当前主key链上已经有元素
            if (bHead)
            {
                // 挂在最前面
                _pMap->saveValue(&getBlockHead(mainKey.getHeadPtr()->_iBlockHead)->_iMKBlockPrev, _iHead);
                _pMap->saveValue(&getBlockHead()->_iMKBlockNext, mainKey.getHeadPtr()->_iBlockHead);
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockHead, _iHead);
                _pMap->saveValue(&getBlockHead()->_iMKBlockPrev, (uint32_t)0);
            }
            else
            {
                // 挂到最后面
                _pMap->saveValue(&getBlockHead()->_iMKBlockNext, (uint32_t)0);
                _pMap->saveValue(&getBlockHead()->_iMKBlockPrev, mainKey.getHeadPtr()->_iBlockTail);
                _pMap->saveValue(&getBlockHead(mainKey.getHeadPtr()->_iBlockTail)->_iMKBlockNext, _iHead);
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockTail, _iHead);
            }
        }

        // 挂在Set链表的头部
        if (_pMap->_pHead->_iSetHead == 0)
        {
            assert(_pMap->_pHead->_iSetTail == 0);
            _pMap->saveValue(&_pMap->_pHead->_iSetHead, _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iSetTail, _iHead);
        }
        else
        {
            assert(_pMap->_pHead->_iSetTail != 0);
            _pMap->saveValue(&getBlockHead()->_iSetNext, _pMap->_pHead->_iSetHead);
            _pMap->saveValue(&getBlockHead(_pMap->_pHead->_iSetHead)->_iSetPrev, _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iSetHead, _iHead);
        }
    }

    void TC_Multi_HashMap_Malloc::Block::makeNew(uint32_t iMainKeyAddr, uint32_t uIndex, Block &block)
    {
        //getBlockHead()->_iSize          = iAllocSize;
        getBlockHead()->_iIndex = uIndex;
        getBlockHead()->_iUKBlockNext = 0;
        getBlockHead()->_iUKBlockPrev = 0;
        getBlockHead()->_iMKBlockNext = 0;
        getBlockHead()->_iMKBlockPrev = 0;
        getBlockHead()->_iSetNext = 0;
        getBlockHead()->_iSetPrev = 0;
        getBlockHead()->_iMainKey = iMainKeyAddr;
        getBlockHead()->_iSyncTime = block.getBlockHead()->_iSyncTime;
        getBlockHead()->_iExpireTime = block.getBlockHead()->_iExpireTime;
        getBlockHead()->_iDataLen = 0;
        getBlockHead()->_iVersion = block.getBlockHead()->_iVersion;		// 有效版本范围1-255
        getBlockHead()->_iBitset = 0;        //把bitset设为0
        SETONLYKEY(false);
        SETDIRTY(block.ISDIRTY());
        SETNEXTCHUNK(false);
        //delete标志设为false，若与原block保持一致(例如原来是delete), 那么TC_Multi_HashMap_Malloc
        //中再次setDelete()时，主key下的block count会再次加1
        SETDELETE(false);

        if (block.ISDIRTY())
            _pMap->incDirtyCount();

        _pMap->incElementCount();
        _pMap->incListCount(uIndex);

        // 增加主key下面的block数量
        MainKey mainKey(_pMap, iMainKeyAddr);
        _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);
        _pMap->updateMaxMainKeyBlockCount(mainKey.getHeadPtr()->_iBlockCount);

        // 挂在block链表上
        if (_pMap->item(uIndex)->_iBlockAddr == 0)
        {
            // 当前hash桶没有元素
            _pMap->saveValue(&_pMap->item(uIndex)->_iBlockAddr, _iHead);
            _pMap->saveValue(&getBlockHead()->_iUKBlockNext, (uint32_t)0);
            _pMap->saveValue(&getBlockHead()->_iUKBlockPrev, (uint32_t)0);
        }
        else
        {
            // 当前hash桶有元素, 挂在桶开头
            _pMap->saveValue(&getBlockHead(_pMap->item(uIndex)->_iBlockAddr)->_iUKBlockPrev, _iHead);
            _pMap->saveValue(&getBlockHead()->_iUKBlockNext, _pMap->item(uIndex)->_iBlockAddr);
            _pMap->saveValue(&_pMap->item(uIndex)->_iBlockAddr, _iHead);
            _pMap->saveValue(&getBlockHead()->_iUKBlockPrev, (uint32_t)0);
        }

        assert(mainKey.getHeadPtr()->_iBlockHead != 0);
        // 挂在主key链上
        // 当前主key链上已经有元素
        if (block.getBlockHead()->_iMKBlockPrev == 0)
        {//这个block前面没数据了
            // 挂在最前面
            _pMap->saveValue(&getBlockHead(mainKey.getHeadPtr()->_iBlockHead)->_iMKBlockPrev, _iHead);
            _pMap->saveValue(&getBlockHead()->_iMKBlockNext, mainKey.getHeadPtr()->_iBlockHead);
            _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockHead, _iHead);
            _pMap->saveValue(&getBlockHead()->_iMKBlockPrev, (uint32_t)0);
        }
        else
        {
            _pMap->saveValue(&getBlockHead()->_iMKBlockNext, block.getHead());
            _pMap->saveValue(&getBlockHead()->_iMKBlockPrev, block.getBlockHead()->_iMKBlockPrev);
            _pMap->saveValue(&getBlockHead(block.getBlockHead()->_iMKBlockPrev)->_iMKBlockNext, _iHead);
            _pMap->saveValue(&block.getBlockHead()->_iMKBlockPrev, _iHead);
        }

        // 挂在Set链表的头部
        if (_pMap->_pHead->_iSetHead == 0)
        {
            assert(_pMap->_pHead->_iSetTail == 0);
            _pMap->saveValue(&_pMap->_pHead->_iSetHead, _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iSetTail, _iHead);
        }
        else
        {
            assert(_pMap->_pHead->_iSetTail != 0);
            _pMap->saveValue(&getBlockHead()->_iSetNext, _pMap->_pHead->_iSetHead);
            _pMap->saveValue(&getBlockHead(_pMap->_pHead->_iSetHead)->_iSetPrev, _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iSetHead, _iHead);
        }
    }

    void TC_Multi_HashMap_Malloc::Block::makeListNew(uint32_t iMainKeyAddr, bool bHead)
    {
        //getBlockHead()->_iSize          = iAllocSize;
        getListBlockHead()->_iMKBlockNext = 0;
        getListBlockHead()->_iMKBlockPrev = 0;
        getListBlockHead()->_iMainKey = iMainKeyAddr;
        getListBlockHead()->_iDataLen = 0;
        getListBlockHead()->_iExpireTime = 0;
        getListBlockHead()->_iBitset = 0;        //把bitset设为0
        SETONLYKEYLIST(false);

        _pMap->incElementCount();

        // 增加主key下面的block数量
        MainKey mainKey(_pMap, iMainKeyAddr);
        _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);
        _pMap->updateMaxMainKeyBlockCount(mainKey.getHeadPtr()->_iBlockCount);

        // 挂在主key链上
        if (mainKey.getHeadPtr()->_iBlockHead == 0)
        {
            // 当前主key链上还没有元素
            _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockHead, _iHead);
            _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockTail, _iHead);
            _pMap->saveValue(&getListBlockHead()->_iMKBlockNext, (uint32_t)0);
            _pMap->saveValue(&getListBlockHead()->_iMKBlockPrev, (uint32_t)0);

            // 减主key的only key计数
            _pMap->delOnlyKeyCountM();
        }
        else
        {
            // 当前主key链上已经有元素
            if (bHead)
            {
                // 挂在最前面
                _pMap->saveValue(&getListBlockHead(mainKey.getHeadPtr()->_iBlockHead)->_iMKBlockPrev, _iHead);
                _pMap->saveValue(&getListBlockHead()->_iMKBlockNext, mainKey.getHeadPtr()->_iBlockHead);
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockHead, _iHead);
                _pMap->saveValue(&getListBlockHead()->_iMKBlockPrev, (uint32_t)0);
            }
            else
            {
                // 挂到最后面
                _pMap->saveValue(&getListBlockHead()->_iMKBlockNext, (uint32_t)0);
                _pMap->saveValue(&getListBlockHead()->_iMKBlockPrev, mainKey.getHeadPtr()->_iBlockTail);
                _pMap->saveValue(&getListBlockHead(mainKey.getHeadPtr()->_iBlockTail)->_iMKBlockNext, _iHead);
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockTail, _iHead);
            }
        }
    }

    void TC_Multi_HashMap_Malloc::Block::makeZSetNew(uint32_t iMainKeyAddr, uint32_t uIndex, bool bHead)
    {
        //getBlockHead()->_iSize          = iAllocSize;
        getBlockHead()->_iIndex = uIndex;
        getBlockHead()->_iUKBlockNext = 0;
        getBlockHead()->_iUKBlockPrev = 0;
        getBlockHead()->_iMKBlockNext = 0;
        getBlockHead()->_iMKBlockPrev = 0;
        getBlockHead()->_iSetNext = 0;
        getBlockHead()->_iSetPrev = 0;
        getBlockHead()->_iMainKey = iMainKeyAddr;
        getBlockHead()->_iSyncTime = 0;
        getBlockHead()->_iExpireTime = 0;
        getBlockHead()->_iDataLen = 0;
        getBlockHead()->_iVersion = 1;		// 有效版本范围1-255
        getBlockHead()->_iBitset = 0;        //把bitset设为0
        SETONLYKEY(false);
        SETDIRTY(true);
        SETNEXTCHUNK(false);
        SETDELETE(false);

        _pMap->incDirtyCount();
        _pMap->incElementCount();
        _pMap->incListCount(uIndex);

        // 增加主key下面的block数量
        MainKey mainKey(_pMap, iMainKeyAddr);
        _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);
        _pMap->updateMaxMainKeyBlockCount(mainKey.getHeadPtr()->_iBlockCount);

        // 挂在block链表上
        if (_pMap->item(uIndex)->_iBlockAddr == 0)
        {
            // 当前hash桶没有元素
            _pMap->saveValue(&_pMap->item(uIndex)->_iBlockAddr, _iHead);
            _pMap->saveValue(&getBlockHead()->_iUKBlockNext, (uint32_t)0);
            _pMap->saveValue(&getBlockHead()->_iUKBlockPrev, (uint32_t)0);
        }
        else
        {
            // 当前hash桶有元素, 挂在桶开头
            _pMap->saveValue(&getBlockHead(_pMap->item(uIndex)->_iBlockAddr)->_iUKBlockPrev, _iHead);
            _pMap->saveValue(&getBlockHead()->_iUKBlockNext, _pMap->item(uIndex)->_iBlockAddr);
            _pMap->saveValue(&_pMap->item(uIndex)->_iBlockAddr, _iHead);
            _pMap->saveValue(&getBlockHead()->_iUKBlockPrev, (uint32_t)0);
        }

        // 挂在Set链表的头部
        if (_pMap->_pHead->_iSetHead == 0)
        {
            assert(_pMap->_pHead->_iSetTail == 0);
            _pMap->saveValue(&_pMap->_pHead->_iSetHead, _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iSetTail, _iHead);
        }
        else
        {
            assert(_pMap->_pHead->_iSetTail != 0);
            _pMap->saveValue(&getBlockHead()->_iSetNext, _pMap->_pHead->_iSetHead);
            _pMap->saveValue(&getBlockHead(_pMap->_pHead->_iSetHead)->_iSetPrev, _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iSetHead, _iHead);
        }
    }

    void TC_Multi_HashMap_Malloc::Block::erase(bool bCheckMainKey /*= true*/)
    {
        // 切换当前使用的数据保护区，因为block::erase操作内部会update
        _pMap->_pstCurrModify = _pMap->_pstInnerModify;

        //////////////////修改脏数据链表/////////////
        if (_pMap->_pHead->_iDirtyTail == _iHead)
        {
            _pMap->saveValue(&_pMap->_pHead->_iDirtyTail, getBlockHead()->_iSetPrev);
        }

        //////////////////修改回写数据链表/////////////
        if (_pMap->_pHead->_iSyncTail == _iHead)
        {
            _pMap->saveValue(&_pMap->_pHead->_iSyncTail, getBlockHead()->_iSetPrev);
        }

        ////////////////////修改Set链表的数据//////////
        {
            bool bHead = (_pMap->_pHead->_iSetHead == _iHead);
            bool bTail = (_pMap->_pHead->_iSetTail == _iHead);

            if (!bHead)
            {
                if (bTail)
                {
                    assert(getBlockHead()->_iSetNext == 0);
                    // 是尾部, 尾部指针指向上一个元素
                    _pMap->saveValue(&_pMap->_pHead->_iSetTail, getBlockHead()->_iSetPrev);
                    _pMap->saveValue(&getBlockHead(getBlockHead()->_iSetPrev)->_iSetNext, (uint32_t)0);
                }
                else
                {
                    // 不是头部也不是尾部
                    assert(getBlockHead()->_iSetNext != 0);
                    _pMap->saveValue(&getBlockHead(getBlockHead()->_iSetPrev)->_iSetNext, getBlockHead()->_iSetNext);
                    _pMap->saveValue(&getBlockHead(getBlockHead()->_iSetNext)->_iSetPrev, getBlockHead()->_iSetPrev);
                }
            }
            else
            {
                if (bTail)
                {
                    assert(getBlockHead()->_iSetNext == 0);
                    assert(getBlockHead()->_iSetPrev == 0);
                    // 头部也是尾部, 指针都设置为0
                    _pMap->saveValue(&_pMap->_pHead->_iSetHead, (uint32_t)0);
                    _pMap->saveValue(&_pMap->_pHead->_iSetTail, (uint32_t)0);
                }
                else
                {
                    // 头部不是尾部, 头部指针指向下一个元素
                    assert(getBlockHead()->_iSetNext != 0);
                    _pMap->saveValue(&_pMap->_pHead->_iSetHead, getBlockHead()->_iSetNext);
                    // 下一个元素上指针为0
                    _pMap->saveValue(&getBlockHead(getBlockHead()->_iSetNext)->_iSetPrev, (uint32_t)0);
                }
            }
        }

        ////////////////////修改主key链表的数据//////////
        //
        {
            MainKey mainKey(_pMap, getBlockHead()->_iMainKey);
            if (getBlockHead()->_iMKBlockPrev != 0)
            {
                // 将上一个block指向下一个
                _pMap->saveValue(&getBlockHead(getBlockHead()->_iMKBlockPrev)->_iMKBlockNext, getBlockHead()->_iMKBlockNext);
            }
            else
            {
                // 主key链上的第一个
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockHead, getBlockHead()->_iMKBlockNext);

                if (mainKey.getHeadPtr()->_iBlockHead == 0)
                {
                    // 主key变成only key了，增加计数
                    _pMap->incOnlyKeyCountM();
                }
            }
            if (getBlockHead()->_iMKBlockNext != 0)
            {
                // 将下一个block指向上一个
                _pMap->saveValue(&getBlockHead(getBlockHead()->_iMKBlockNext)->_iMKBlockPrev, getBlockHead()->_iMKBlockPrev);
            }
            else
            {
                // 主key链上的最后一个
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockTail, getBlockHead()->_iMKBlockPrev);
            }
        }

        ///////////////////从block链表中去掉///////////
        //
        //上一个block指向下一个block
        if (getBlockHead()->_iUKBlockPrev != 0)
        {
            _pMap->saveValue(&getBlockHead(getBlockHead()->_iUKBlockPrev)->_iUKBlockNext, getBlockHead()->_iUKBlockNext);
        }

        //下一个block指向上一个
        if (getBlockHead()->_iUKBlockNext != 0)
        {
            _pMap->saveValue(&getBlockHead(getBlockHead()->_iUKBlockNext)->_iUKBlockPrev, getBlockHead()->_iUKBlockPrev);
        }

        //////////////////如果是hash头部, 需要修改hash索引数据指针//////
        //
        _pMap->delListCount(getBlockHead()->_iIndex);
        if (getBlockHead()->_iUKBlockPrev == 0)
        {
            //如果是hash桶的头部, 则还需要处理
            TC_Multi_HashMap_Malloc::tagHashItem *pItem = _pMap->item(getBlockHead()->_iIndex);
            assert(pItem->_iBlockAddr == _iHead);
            if (pItem->_iBlockAddr == _iHead)
            {
                _pMap->saveValue(&pItem->_iBlockAddr, getBlockHead()->_iUKBlockNext);
            }
        }

        //////////////////脏数据///////////////////
        //
        if (isDirty())
        {
            _pMap->delDirtyCount();
        }

        if (isOnlyKey())
        {
            _pMap->delOnlyKeyCount();
        }

        //元素个数减少
        _pMap->delElementCount();

        // 减少主key下数据个数
        MainKey mainKey(_pMap, getBlockHead()->_iMainKey);
        _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount - 1);
        _pMap->updateMaxMainKeyBlockCount(mainKey.getHeadPtr()->_iBlockCount);

        //保存block地址，成功后即使再core该block也会被删除
        _pMap->saveAddr(_iHead, -2);

        //使修改生效，即使这过程中core也能保证block被删除
        _pMap->doUpdate4();

        _pMap->_pstCurrModify = _pMap->_pstOuterModify;

        // 如果由于主key链下的数据全部被删除了，主key头也必须删除以释放主key头空间
        if (bCheckMainKey && mainKey.getHeadPtr()->_iBlockCount <= 0)
        {
            vector<Value> vtData;
            mainKey.erase(vtData);
        }
    }

    void TC_Multi_HashMap_Malloc::Block::eraseList(bool bCheckMainKey)
    {
        // 切换当前使用的数据保护区，因为block::erase操作内部会update
        _pMap->_pstCurrModify = _pMap->_pstInnerModify;

        ////////////////////修改主key链表的数据//////////
        //
        {
            MainKey mainKey(_pMap, getListBlockHead()->_iMainKey);
            if (getListBlockHead()->_iMKBlockPrev != 0)
            {
                // 将上一个block指向下一个
                _pMap->saveValue(&getListBlockHead(getListBlockHead()->_iMKBlockPrev)->_iMKBlockNext, getListBlockHead()->_iMKBlockNext);
            }
            else
            {
                // 主key链上的第一个
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockHead, getListBlockHead()->_iMKBlockNext);

                if (mainKey.getHeadPtr()->_iBlockHead == 0)
                {
                    // 主key变成only key了，增加计数
                    _pMap->incOnlyKeyCountM();
                }
            }
            if (getListBlockHead()->_iMKBlockNext != 0)
            {
                // 将下一个block指向上一个
                _pMap->saveValue(&getListBlockHead(getListBlockHead()->_iMKBlockNext)->_iMKBlockPrev, getListBlockHead()->_iMKBlockPrev);
            }
            else
            {
                // 主key链上的最后一个
                _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockTail, getListBlockHead()->_iMKBlockPrev);
            }
        }

        //元素个数减少
        _pMap->delElementCount();

        // 减少主key下数据个数
        MainKey mainKey(_pMap, getListBlockHead()->_iMainKey);
        _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount - 1);
        _pMap->updateMaxMainKeyBlockCount(mainKey.getHeadPtr()->_iBlockCount);

        //保存block地址，成功后即使再core该block也会被删除
        _pMap->saveAddr(_iHead, -2);

        //使修改生效，即使这过程中core也能保证block被删除
        _pMap->doUpdate4();

        _pMap->_pstCurrModify = _pMap->_pstOuterModify;

        // 如果由于主key链下的数据全部被删除了，主key头也必须删除以释放主key头空间
        if (bCheckMainKey && mainKey.getHeadPtr()->_iBlockCount <= 0)
        {
            vector<Value> vtData;
            mainKey.erase(vtData);
        }
    }

    void TC_Multi_HashMap_Malloc::Block::eraseZSet(bool bCheckMainKey)
    {
        // 切换当前使用的数据保护区，因为block::erase操作内部会update
        _pMap->_pstCurrModify = _pMap->_pstInnerModify;

        {
            MainKey mainKey(_pMap, getBlockHead()->_iMainKey);
            _pMap->delSkipList(mainKey, getHead());
        }

        //////////////////修改脏数据链表/////////////
        if (_pMap->_pHead->_iDirtyTail == _iHead)
        {
            _pMap->saveValue(&_pMap->_pHead->_iDirtyTail, getBlockHead()->_iSetPrev);
        }

        //////////////////修改回写数据链表/////////////
        if (_pMap->_pHead->_iSyncTail == _iHead)
        {
            _pMap->saveValue(&_pMap->_pHead->_iSyncTail, getBlockHead()->_iSetPrev);
        }

        ////////////////////修改Set链表的数据//////////
        {
            bool bHead = (_pMap->_pHead->_iSetHead == _iHead);
            bool bTail = (_pMap->_pHead->_iSetTail == _iHead);

            if (!bHead)
            {
                if (bTail)
                {
                    assert(getBlockHead()->_iSetNext == 0);
                    // 是尾部, 尾部指针指向上一个元素
                    _pMap->saveValue(&_pMap->_pHead->_iSetTail, getBlockHead()->_iSetPrev);
                    _pMap->saveValue(&getBlockHead(getBlockHead()->_iSetPrev)->_iSetNext, (uint32_t)0);
                }
                else
                {
                    // 不是头部也不是尾部
                    assert(getBlockHead()->_iSetNext != 0);
                    _pMap->saveValue(&getBlockHead(getBlockHead()->_iSetPrev)->_iSetNext, getBlockHead()->_iSetNext);
                    _pMap->saveValue(&getBlockHead(getBlockHead()->_iSetNext)->_iSetPrev, getBlockHead()->_iSetPrev);
                }
            }
            else
            {
                if (bTail)
                {
                    assert(getBlockHead()->_iSetNext == 0);
                    assert(getBlockHead()->_iSetPrev == 0);
                    // 头部也是尾部, 指针都设置为0
                    _pMap->saveValue(&_pMap->_pHead->_iSetHead, (uint32_t)0);
                    _pMap->saveValue(&_pMap->_pHead->_iSetTail, (uint32_t)0);
                }
                else
                {
                    // 头部不是尾部, 头部指针指向下一个元素
                    assert(getBlockHead()->_iSetNext != 0);
                    _pMap->saveValue(&_pMap->_pHead->_iSetHead, getBlockHead()->_iSetNext);
                    // 下一个元素上指针为0
                    _pMap->saveValue(&getBlockHead(getBlockHead()->_iSetNext)->_iSetPrev, (uint32_t)0);
                }
            }
        }

        ///////////////////从block链表中去掉///////////
        //
        //上一个block指向下一个block
        if (getBlockHead()->_iUKBlockPrev != 0)
        {
            _pMap->saveValue(&getBlockHead(getBlockHead()->_iUKBlockPrev)->_iUKBlockNext, getBlockHead()->_iUKBlockNext);
        }

        //下一个block指向上一个
        if (getBlockHead()->_iUKBlockNext != 0)
        {
            _pMap->saveValue(&getBlockHead(getBlockHead()->_iUKBlockNext)->_iUKBlockPrev, getBlockHead()->_iUKBlockPrev);
        }

        //////////////////如果是hash头部, 需要修改hash索引数据指针//////
        //
        _pMap->delListCount(getBlockHead()->_iIndex);
        if (getBlockHead()->_iUKBlockPrev == 0)
        {
            //如果是hash桶的头部, 则还需要处理
            TC_Multi_HashMap_Malloc::tagHashItem *pItem = _pMap->item(getBlockHead()->_iIndex);
            assert(pItem->_iBlockAddr == _iHead);
            if (pItem->_iBlockAddr == _iHead)
            {
                _pMap->saveValue(&pItem->_iBlockAddr, getBlockHead()->_iUKBlockNext);
            }
        }

        //////////////////脏数据///////////////////
        //
        if (isDirty())
        {
            _pMap->delDirtyCount();
        }

        if (isOnlyKey())
        {
            _pMap->delOnlyKeyCount();
        }

        //元素个数减少
        _pMap->delElementCount();

        // 减少主key下数据个数
        MainKey mainKey(_pMap, getBlockHead()->_iMainKey);
        _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount - 1);
        _pMap->updateMaxMainKeyBlockCount(mainKey.getHeadPtr()->_iBlockCount);

        //保存block地址，成功后即使再core该block也会被删除
        _pMap->saveAddr(_iHead, -2);

        //使修改生效，即使这过程中core也能保证block被删除
        _pMap->doUpdate4();

        _pMap->_pstCurrModify = _pMap->_pstOuterModify;

        if (0 == mainKey.getNext(0))
            _pMap->incOnlyKeyCountM();

        // 如果由于主key链下的数据全部被删除了，主key头也必须删除以释放主key头空间
        if (bCheckMainKey && mainKey.getNext(0) == 0)
        {
            vector<Value> vtData;
            mainKey.erase(vtData);
        }
    }

    void TC_Multi_HashMap_Malloc::Block::refreshSetList()
    {
        assert(_pMap->_pHead->_iSetHead != 0);
        assert(_pMap->_pHead->_iSetTail != 0);

        //修改同步链表
        if (_pMap->_pHead->_iSyncTail == _iHead && _pMap->_pHead->_iSetHead != _iHead)
        {
            _pMap->saveValue(&_pMap->_pHead->_iSyncTail, getBlockHead()->_iSetPrev);
        }

        //修改脏数据尾部链表指针, 不仅一个元素
        if (_pMap->_pHead->_iDirtyTail == _iHead && _pMap->_pHead->_iSetHead != _iHead)
        {
            //脏数据尾部位置前移
            _pMap->saveValue(&_pMap->_pHead->_iDirtyTail, getBlockHead()->_iSetPrev);
        }
        else if (_pMap->_pHead->_iDirtyTail == 0)
        {
            //原来没有脏数据
            _pMap->saveValue(&_pMap->_pHead->_iDirtyTail, _iHead);
        }

        //是头部数据或者set新数据时走到这个分支
        if (_pMap->_pHead->_iSetHead == _iHead)
        {
            //刷新Get链
            MainKey mainKey(_pMap, getBlockHead()->_iMainKey);
            mainKey.refreshGetList();
            return;
        }

        uint32_t iPrev = getBlockHead()->_iSetPrev;
        uint32_t iNext = getBlockHead()->_iSetNext;

        assert(iPrev != 0);

        //挂在链表头部
        _pMap->saveValue(&getBlockHead()->_iSetNext, _pMap->_pHead->_iSetHead);
        _pMap->saveValue(&getBlockHead(_pMap->_pHead->_iSetHead)->_iSetPrev, _iHead);
        _pMap->saveValue(&_pMap->_pHead->_iSetHead, _iHead);
        _pMap->saveValue(&getBlockHead()->_iSetPrev, (uint32_t)0);

        //上一个元素的Next指针指向下一个元素
        _pMap->saveValue(&getBlockHead(iPrev)->_iSetNext, iNext);

        //下一个元素的Prev指向上一个元素
        if (iNext != 0)
        {
            _pMap->saveValue(&getBlockHead(iNext)->_iSetPrev, iPrev);
        }
        else
        {
            //改变尾部指针
            assert(_pMap->_pHead->_iSetTail == _iHead);
            _pMap->saveValue(&_pMap->_pHead->_iSetTail, iPrev);
        }

        //刷新Get链
        MainKey mainKey(_pMap, getBlockHead()->_iMainKey);
        mainKey.refreshGetList();
    }

    void TC_Multi_HashMap_Malloc::Block::refreshKeyList(bool bHead)
    {
        MainKey mainKey(_pMap, getBlockHead()->_iMainKey);

        if (bHead)
        {
            //已经是头了
            if (mainKey.getHeadPtr()->_iBlockHead == _iHead)
                return;
        }
        else
        {
            //已经是尾了
            if (mainKey.getHeadPtr()->_iBlockTail == _iHead)
                return;
        }

        //从现有的链中抽出
        if (mainKey.getHeadPtr()->_iBlockHead == _iHead)
            _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockHead, getBlockHead()->_iMKBlockNext);
        else if (mainKey.getHeadPtr()->_iBlockTail == _iHead)
            _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockTail, getBlockHead()->_iMKBlockPrev);

        if (getBlockHead()->_iMKBlockPrev != 0)
            _pMap->saveValue(&getBlockHead(getBlockHead()->_iMKBlockPrev)->_iMKBlockNext, getBlockHead()->_iMKBlockNext);
        if (getBlockHead()->_iMKBlockNext != 0)
            _pMap->saveValue(&getBlockHead(getBlockHead()->_iMKBlockNext)->_iMKBlockPrev, getBlockHead()->_iMKBlockPrev);

        if (bHead)
        {
            // 挂在最前面
            _pMap->saveValue(&getBlockHead(mainKey.getHeadPtr()->_iBlockHead)->_iMKBlockPrev, _iHead);
            _pMap->saveValue(&getBlockHead()->_iMKBlockNext, mainKey.getHeadPtr()->_iBlockHead);
            _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockHead, _iHead);
            _pMap->saveValue(&getBlockHead()->_iMKBlockPrev, (uint32_t)0);
        }
        else
        {
            // 挂到最后面
            _pMap->saveValue(&getBlockHead()->_iMKBlockNext, (uint32_t)0);
            _pMap->saveValue(&getBlockHead()->_iMKBlockPrev, mainKey.getHeadPtr()->_iBlockTail);
            _pMap->saveValue(&getBlockHead(mainKey.getHeadPtr()->_iBlockTail)->_iMKBlockNext, _iHead);
            _pMap->saveValue(&mainKey.getHeadPtr()->_iBlockTail, _iHead);
        }
    }

    void TC_Multi_HashMap_Malloc::Block::deallocate(uint32_t iChunk)
    {
        vector<MemChunk> v;
        MemChunk chunk;

        tagChunkHead *pChunk = getChunkHead(iChunk);
        chunk._iAddr = iChunk;
        chunk._iSize = pChunk->_iSize;
        v.push_back(chunk);

        //获取所有后续的chunk地址
        while (true)
        {
            if (pChunk->_bNextChunk)
            {
                chunk._iAddr = pChunk->_iNextChunk;
                pChunk = getChunkHead(pChunk->_iNextChunk);
                chunk._iSize = pChunk->_iSize;
                v.push_back(chunk);
            }
            else
            {
                break;
            }
        }

        //空间全部释放掉
        _pMap->_pDataAllocator->deallocateMemChunk(v, false);
    }

    int TC_Multi_HashMap_Malloc::Block::allocate(uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        uint32_t fn = 0;

        // 第一个chunk的有效数据容量
        fn = getBlockHead()->_iSize - sizeof(tagBlockHead);
        if (fn >= iDataLen)
        {
            // 一个chunk就可以了, 后续的chunk都要释放掉
            if (HASNEXTCHUNK())
            {
                uint32_t iNextChunk = getBlockHead()->_iNextChunk;

                // 保存第一个chunk的地址
                _pMap->saveAddr(iNextChunk, -1);
                // 然后修改自己的区块
                _pMap->saveValue(&getBlockHead()->_iBitset, NEXTCHUNK_BIT, false);
                _pMap->saveValue(&getBlockHead()->_iDataLen, (uint32_t)0);
                // 修改成功后再释放区块, 从而保证, 不会core的时候导致整个内存块不可用
                _pMap->doUpdate3();
            }
            return TC_Multi_HashMap_Malloc::RT_OK;
        }

        // 计算还需要多少长度
        fn = iDataLen - fn;

        if (HASNEXTCHUNK())
        {
            tagChunkHead *pChunk = getChunkHead(getBlockHead()->_iNextChunk);

            while (true)
            {
                if (fn <= (pChunk->_iSize - sizeof(tagChunkHead)))
                {
                    //已经不需要chunk了, 释放多余的chunk
                    if (pChunk->_bNextChunk)
                    {
                        uint32_t iNextChunk = pChunk->_iNextChunk;

                        // 保留第一个chunk的地址
                        _pMap->saveAddr(iNextChunk, -1);
                        // 然后修改自己的区块
                        _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)false);
                        _pMap->saveValue(&pChunk->_iDataLen, (uint32_t)0);
                        // 修改成功后再释放内存快，从而保证在异常core时不会导致整个内存块不可用
                        _pMap->doUpdate3();
                    }
                    return TC_Multi_HashMap_Malloc::RT_OK;
                }

                //计算, 还需要多少长度
                fn -= pChunk->_iSize - sizeof(tagChunkHead);

                if (pChunk->_bNextChunk)
                {
                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
                else
                {
                    //没有后续chunk了, 需要新分配fn的空间
                    vector<uint32_t> chunks;
                    int ret = allocateChunk(fn, chunks, vtData);
                    if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //没有内存可以分配
                        return ret;
                    }

                    _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)true);
                    _pMap->saveValue(&pChunk->_iNextChunk, chunks[0]);
                    //chunk指向分配的第一个chunk
                    pChunk = getChunkHead(chunks[0]);

                    //modified by smitchzhao @ 2010-8-18
                    pChunk->_bNextChunk = false;
                    pChunk->_iDataLen = (uint32_t)0;
                    _pMap->saveAddr(chunks[0], 0);

                    //连接每个chunk
                    return joinChunk(pChunk, chunks);
                }
            }
        }
        else
        {
            //没有后续chunk了, 需要新分配fn空间
            vector<uint32_t> chunks;
            int ret = allocateChunk(fn, chunks, vtData);
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                //没有内存可以分配
                return ret;
            }

            _pMap->saveValue(&getBlockHead()->_iBitset, NEXTCHUNK_BIT, true);
            _pMap->saveValue(&getBlockHead()->_iNextChunk, chunks[0]);
            //chunk指向分配的第一个chunk
            tagChunkHead *pChunk = getChunkHead(chunks[0]);

            pChunk->_bNextChunk = false;
            pChunk->_iDataLen = (uint32_t)0;
            _pMap->saveAddr(chunks[0], 0);

            //连接每个chunk
            return joinChunk(pChunk, chunks);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::allocateList(uint32_t iDataLen, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        uint32_t fn = 0;

        // 第一个chunk的有效数据容量
        fn = getListBlockHead()->_iSize - sizeof(tagListBlockHead);
        if (fn >= iDataLen)
        {
            // 一个chunk就可以了, 后续的chunk都要释放掉
            if (HASNEXTCHUNKLIST())
            {
                uint32_t iNextChunk = getListBlockHead()->_iNextChunk;

                // 保存第一个chunk的地址
                _pMap->saveAddr(iNextChunk, -1);
                // 然后修改自己的区块
                _pMap->saveValue(&getListBlockHead()->_iBitset, NEXTCHUNK_BIT, false);
                _pMap->saveValue(&getListBlockHead()->_iDataLen, (uint32_t)0);
                // 修改成功后再释放区块, 从而保证, 不会core的时候导致整个内存块不可用
                _pMap->doUpdate3();
            }
            return TC_Multi_HashMap_Malloc::RT_OK;
        }

        // 计算还需要多少长度
        fn = iDataLen - fn;

        if (HASNEXTCHUNKLIST())
        {
            tagChunkHead *pChunk = getChunkHead(getListBlockHead()->_iNextChunk);

            while (true)
            {
                if (fn <= (pChunk->_iSize - sizeof(tagChunkHead)))
                {
                    //已经不需要chunk了, 释放多余的chunk
                    if (pChunk->_bNextChunk)
                    {
                        uint32_t iNextChunk = pChunk->_iNextChunk;

                        // 保留第一个chunk的地址
                        _pMap->saveAddr(iNextChunk, -1);
                        // 然后修改自己的区块
                        _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)false);
                        _pMap->saveValue(&pChunk->_iDataLen, (uint32_t)0);
                        // 修改成功后再释放内存快，从而保证在异常core时不会导致整个内存块不可用
                        _pMap->doUpdate3();
                    }
                    return TC_Multi_HashMap_Malloc::RT_OK;
                }

                //计算, 还需要多少长度
                fn -= pChunk->_iSize - sizeof(tagChunkHead);

                if (pChunk->_bNextChunk)
                {
                    pChunk = getChunkHead(pChunk->_iNextChunk);
                }
                else
                {
                    //没有后续chunk了, 需要新分配fn的空间
                    vector<uint32_t> chunks;
                    int ret = allocateChunkList(fn, chunks, vtData);
                    if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //没有内存可以分配
                        return ret;
                    }

                    _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)true);
                    _pMap->saveValue(&pChunk->_iNextChunk, chunks[0]);
                    //chunk指向分配的第一个chunk
                    pChunk = getChunkHead(chunks[0]);

                    //modified by smitchzhao @ 2010-8-18
                    pChunk->_bNextChunk = false;
                    pChunk->_iDataLen = (uint32_t)0;
                    _pMap->saveAddr(chunks[0], 0);

                    //连接每个chunk
                    return joinChunk(pChunk, chunks);
                }
            }
        }
        else
        {
            //没有后续chunk了, 需要新分配fn空间
            vector<uint32_t> chunks;
            int ret = allocateChunkList(fn, chunks, vtData);
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                //没有内存可以分配
                return ret;
            }

            _pMap->saveValue(&getListBlockHead()->_iBitset, NEXTCHUNK_BIT, true);
            _pMap->saveValue(&getListBlockHead()->_iNextChunk, chunks[0]);
            //chunk指向分配的第一个chunk
            tagChunkHead *pChunk = getChunkHead(chunks[0]);

            pChunk->_bNextChunk = false;
            pChunk->_iDataLen = (uint32_t)0;
            _pMap->saveAddr(chunks[0], 0);

            //连接每个chunk
            return joinChunk(pChunk, chunks);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::joinChunk(tagChunkHead *pChunk, const vector<uint32_t>& chunks)
    {
        tagChunkHead* pNextChunk = NULL;
        for (size_t i = 0; i < chunks.size(); ++i)
        {
            if (i == chunks.size() - 1)
            {
                _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)false);
            }
            else
            {
                pNextChunk = getChunkHead(chunks[i + 1]);
                pNextChunk->_bNextChunk = false;
                pNextChunk->_iDataLen = (uint32_t)0;

                _pMap->saveValue(&pChunk->_bNextChunk, (uint8_t)true);
                _pMap->saveValue(&pChunk->_iNextChunk, chunks[i + 1]);
                if (i > 0 && i % 10 == 0)
                {
                    _pMap->doUpdate2();
                }

                pChunk = pNextChunk;
            }
        }

        _pMap->doUpdate2();

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::allocateChunk(uint32_t fn, vector<uint32_t> &chunks, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        assert(fn > 0);

        while (true)
        {
            uint32_t iAllocSize = fn + sizeof(tagChunkHead);
            // 分配空间
            // 正在分配的block所属的主key是不能被淘汰的
            uint32_t t = _pMap->_pDataAllocator->allocateChunk(getBlockHead()->_iMainKey, iAllocSize, vtData);
            if (t == 0)
            {
                //没有内存可以分配了, 先把分配的归还
                _pMap->_pDataAllocator->deallocateChunk(chunks);
                chunks.clear();
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }

            //设置分配的数据块的大小
            getChunkHead(t)->_iSize = iAllocSize;

            chunks.push_back(t);

            //分配够了
            if (fn <= (iAllocSize - sizeof(tagChunkHead)))
            {
                break;
            }

            //还需要多少空间
            fn -= iAllocSize - sizeof(tagChunkHead);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::Block::allocateChunkList(uint32_t fn, vector<uint32_t> &chunks, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        assert(fn > 0);

        while (true)
        {
            uint32_t iAllocSize = fn + sizeof(tagChunkHead);
            // 分配空间
            // 正在分配的block所属的主key是不能被淘汰的
            uint32_t t = _pMap->_pDataAllocator->allocateChunk(getListBlockHead()->_iMainKey, iAllocSize, vtData);
            if (t == 0)
            {
                //没有内存可以分配了, 先把分配的归还
                _pMap->_pDataAllocator->deallocateChunk(chunks);
                chunks.clear();
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }

            //设置分配的数据块的大小
            getChunkHead(t)->_iSize = iAllocSize;

            chunks.push_back(t);

            //分配够了
            if (fn <= (iAllocSize - sizeof(tagChunkHead)))
            {
                break;
            }

            //还需要多少空间
            fn -= iAllocSize - sizeof(tagChunkHead);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    uint32_t TC_Multi_HashMap_Malloc::Block::getDataLen()
    {
        uint32_t n = 0;
        if (!HASNEXTCHUNK())
        {
            n += getBlockHead()->_iDataLen;
            return n;
        }

        //当前块的大小
        n += getBlockHead()->_iSize - sizeof(tagBlockHead);
        tagChunkHead *pChunk = getChunkHead(getBlockHead()->_iNextChunk);

        while (true)
        {
            if (pChunk->_bNextChunk)
            {
                //当前块的大小
                n += pChunk->_iSize - sizeof(tagChunkHead);
                pChunk = getChunkHead(pChunk->_iNextChunk);
            }
            else
            {
                n += pChunk->_iDataLen;
                break;
            }
        }

        return n;
    }

    uint32_t TC_Multi_HashMap_Malloc::Block::getListDataLen()
    {
        uint32_t n = 0;
        if (!HASNEXTCHUNKLIST())
        {
            n += getListBlockHead()->_iDataLen;
            return n;
        }

        //当前块的大小
        n += getListBlockHead()->_iSize - sizeof(tagListBlockHead);
        tagChunkHead *pChunk = getChunkHead(getListBlockHead()->_iNextChunk);

        while (true)
        {
            if (pChunk->_bNextChunk)
            {
                //当前块的大小
                n += pChunk->_iSize - sizeof(tagChunkHead);
                pChunk = getChunkHead(pChunk->_iNextChunk);
            }
            else
            {
                n += pChunk->_iDataLen;
                break;
            }
        }

        return n;
    }

    ////////////////////////////////////////////////////////

    uint32_t TC_Multi_HashMap_Malloc::BlockAllocator::allocateMemBlock(uint8_t type, uint32_t iMainKeyAddr, uint32_t index, bool bHead,
        uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
    begin:
        size_t iPageId, iChunkIndex;
        size_t bigSize = (size_t)((iAllocSize > _pMap->_iMinChunkSize) ? iAllocSize : _pMap->_iMinChunkSize);
        void* pAddr = _pChunkAllocator->allocate(bigSize, bigSize, iPageId, iChunkIndex);
        if (pAddr == NULL)
        {
            size_t ret = _pMap->eraseExcept(iMainKeyAddr, vtData);
            if (ret == 0)
            {
                return 0;     //没有空间可以释放了
            }
            goto begin;
        }
        iAllocSize = (uint32_t)bigSize;
        _pMap->incUsedDataMemSize(iAllocSize);
        _pMap->incChunkCount();

        uint32_t iAddr = (uint32_t)(((iPageId + 1) << 9) | iChunkIndex);
        Block block(_pMap, iAddr);
        if (MainKey::LIST_TYPE != type)
            block.getBlockHead()->_iSize = iAllocSize;
        else
            block.getListBlockHead()->_iSize = iAllocSize;

        _pMap->saveAddr(iAddr, -3);

        assert(iAllocSize >= _pMap->_iMinChunkSize);

        // 初始化block头部信息
        switch (type)
        {
        case MainKey::HASH_TYPE:
        case MainKey::SET_TYPE:
            block.makeNew(iMainKeyAddr, index, bHead);
            break;
        case MainKey::LIST_TYPE:
            block.makeListNew(iMainKeyAddr, bHead);
            break;
        case MainKey::ZSET_TYPE:
            block.makeZSetNew(iMainKeyAddr, index, bHead);
            break;
        }


        return iAddr;
    }

    uint32_t TC_Multi_HashMap_Malloc::BlockAllocator::relocateMemBlock(uint32_t srcAddr, uint8_t iVersion, uint32_t iMainKeyAddr, uint32_t index, bool bHead,
        uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        Block block(_pMap, srcAddr);
        if (iVersion != 0 && block.getBlockHead()->_iVersion != iVersion)
        {
            // 数据版本不匹配就不动了
            return srcAddr;
        }

        size_t bigSize = (size_t)((iAllocSize > _pMap->_iMinChunkSize) ? iAllocSize : _pMap->_iMinChunkSize);
        size_t iClassSize = Static::sizemap()->SizeClass(bigSize);
        size_t size = Static::sizemap()->ByteSizeForClass(iClassSize);

        //先查看数据是否变化
        if (iAllocSize == block.getBlockHead()->_iSize)
            return srcAddr;
        else if (iAllocSize < block.getBlockHead()->_iSize)
        {//如果变小，判断是否小于下一级分配的大小
            if (size == block.getBlockHead()->_iSize)
                return srcAddr;
            else if (size < block.getBlockHead()->_iSize)
            {//重新分配
            }
            else
            {//还会更大？重新分配
            }
        }
        else
        {//如果变大了，重新分配
        }

        size_t iPageId, iChunkIndex;
        void* pAddr = _pChunkAllocator->allocate(bigSize, bigSize, iPageId, iChunkIndex);
        if (pAddr == NULL)
        {
            return srcAddr;     //没有空间可以释放了
        }

        if (bigSize < _pMap->_iMinChunkSize || bigSize < iAllocSize)
        {//分配的空间不够
            _pChunkAllocator->deallocate(iPageId, iChunkIndex);
            return srcAddr;
        }

        _pMap->incUsedDataMemSize(bigSize);
        _pMap->incChunkCount();

        uint32_t iAddr = (uint32_t)(((iPageId + 1) << 9) | iChunkIndex);
        Block newblock(_pMap, iAddr);
        newblock.getBlockHead()->_iSize = bigSize;

        _pMap->saveAddr(iAddr, -3);

        // 初始化block头部信息
        newblock.makeNew(iMainKeyAddr, index, block);

        return iAddr;
    }

    uint32_t TC_Multi_HashMap_Malloc::BlockAllocator::allocateListBlock(uint32_t iMainKeyAddr, bool bHead, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
    begin:
        size_t iPageId, iChunkIndex;
        size_t bigSize = (size_t)((iAllocSize > _pMap->_iMinChunkSize) ? iAllocSize : _pMap->_iMinChunkSize);
        void* pAddr = _pChunkAllocator->allocate(bigSize, bigSize, iPageId, iChunkIndex);
        if (pAddr == NULL)
        {
            size_t ret = _pMap->eraseExcept(iMainKeyAddr, vtData);
            if (ret == 0)
            {
                return 0;     //没有空间可以释放了
            }
            goto begin;
        }
        iAllocSize = (uint32_t)bigSize;
        _pMap->incUsedDataMemSize(iAllocSize);
        _pMap->incChunkCount();

        uint32_t iAddr = (uint32_t)(((iPageId + 1) << 9) | iChunkIndex);
        Block block(_pMap, iAddr);
        block.getBlockHead()->_iSize = iAllocSize;

        _pMap->saveAddr(iAddr, -3);

        assert(iAllocSize >= _pMap->_iMinChunkSize);


        // 初始化block头部信息
        block.makeListNew(iMainKeyAddr, bHead);

        return iAddr;
    }

    uint32_t TC_Multi_HashMap_Malloc::BlockAllocator::allocateMainKeyHead(uint32_t index, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
    begin:
        size_t iPageId, iChunkIndex;
        size_t bigSize = (size_t)((iAllocSize > _pMap->_iMinChunkSize) ? iAllocSize : _pMap->_iMinChunkSize);
        void* pAddr = _pChunkAllocator->allocate(bigSize, bigSize, iPageId, iChunkIndex);
        if (pAddr == NULL)
        {
            size_t ret = _pMap->eraseExcept(0, vtData);
            if (ret == 0)
            {
                return 0;     //没有空间可以释放了
            }
            goto begin;
        }
        iAllocSize = (uint32_t)bigSize;
        _pMap->incMainKeyUsedMemSize(iAllocSize);
        _pMap->incMainKeyChunkCount();

        uint32_t iAddr = (uint32_t)(((iPageId + 1) << 9) | iChunkIndex);
        MainKey mainKey(_pMap, iAddr);
        mainKey.getHeadPtr()->_iSize = iAllocSize;

        _pMap->saveAddr(iAddr, -3, true);

        assert(iAllocSize >= _pMap->_iMinChunkSize);

        // 分配的新的MemBlock, 初始化一下
        mainKey.makeNew(index);

        return iAddr;
    }

    uint32_t TC_Multi_HashMap_Malloc::BlockAllocator::allocateChunk(uint32_t iAddr, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
    begin:
        size_t iPageId, iChunkIndex;
        size_t bigSize = (size_t)((iAllocSize > _pMap->_iMinChunkSize) ? iAllocSize : _pMap->_iMinChunkSize);
        void* pAddr = _pChunkAllocator->allocate(bigSize, bigSize, iPageId, iChunkIndex);
        if (pAddr == NULL)
        {
            size_t ret = _pMap->eraseExcept(iAddr, vtData);
            if (ret == 0)
            {
                return 0;     //没有空间可以释放了
            }
            goto begin;
        }
        iAllocSize = (uint32_t)bigSize;
        if (iAllocSize < _pMap->_iMinChunkSize)
        {
            _pChunkAllocator->deallocate(iPageId, iChunkIndex);
            assert(false);
        }
        _pMap->incUsedDataMemSize(iAllocSize);
        _pMap->incChunkCount();

        return (uint32_t)(((iPageId + 1) << 9) | iChunkIndex);
    }

    uint32_t TC_Multi_HashMap_Malloc::BlockAllocator::allocateMainKeyChunk(uint32_t iAddr, uint32_t &iAllocSize, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
    begin:
        size_t iPageId, iChunkIndex;
        size_t bigSize = (size_t)((iAllocSize > _pMap->_iMinChunkSize) ? iAllocSize : _pMap->_iMinChunkSize);
        void* pAddr = _pChunkAllocator->allocate(bigSize, bigSize, iPageId, iChunkIndex);
        if (pAddr == NULL)
        {
            size_t ret = _pMap->eraseExcept(iAddr, vtData);
            if (ret == 0)
            {
                return 0;     //没有空间可以释放了
            }
            goto begin;
        }
        iAllocSize = (uint32_t)bigSize;
        if (iAllocSize < _pMap->_iMinChunkSize)
        {
            _pChunkAllocator->deallocate(iPageId, iChunkIndex);
            assert(false);
        }
        _pMap->incMainKeyUsedMemSize(iAllocSize);
        _pMap->incMainKeyChunkCount();

        return (uint32_t)(((iPageId + 1) << 9) | iChunkIndex);
    }

    void TC_Multi_HashMap_Malloc::BlockAllocator::deallocateMemChunk(const MemChunk& chunk, bool bMainKey /*= false*/)
    {
        _pChunkAllocator->deallocate((size_t)((chunk._iAddr >> 9) - 1), (size_t)(chunk._iAddr & 0x1FF));
        if (bMainKey)
        {
            _pMap->delMainKeyUsedMemSize(chunk._iSize);
            _pMap->delMainKeyChunkCount();
        }
        else
        {
            _pMap->delUsedDataMemSize(chunk._iSize);
            _pMap->delChunkCount();
        }
    }

    void TC_Multi_HashMap_Malloc::BlockAllocator::deallocateMemChunk(const vector<MemChunk> &vtChunk, bool bMainKey /*= false*/)
    {
        for (size_t i = 0; i < vtChunk.size(); ++i)
        {
            _pChunkAllocator->deallocate((size_t)((vtChunk[i]._iAddr >> 9) - 1), (size_t)(vtChunk[i]._iAddr & 0x1FF));
            if (bMainKey)
            {
                _pMap->delMainKeyUsedMemSize(vtChunk[i]._iSize);
                _pMap->delMainKeyChunkCount();
            }
            else
            {
                _pMap->delUsedDataMemSize(vtChunk[i]._iSize);
                _pMap->delChunkCount();
            }
        }
    }

    void TC_Multi_HashMap_Malloc::BlockAllocator::deallocateMainKeyChunk(const vector<uint32_t> &v)
    {
        for (size_t i = 0; i < v.size(); ++i)
        {
            size_t iSize = (size_t)(((MainKey::tagChunkHead*)_pMap->getMainKeyAbsolute(v[i]))->_iSize);
            _pChunkAllocator->deallocate((size_t)((v[i] >> 9) - 1), (size_t)(v[i] & 0x1FF));
            _pMap->delMainKeyUsedMemSize(iSize);
            _pMap->delMainKeyChunkCount();
        }
    }

    void TC_Multi_HashMap_Malloc::BlockAllocator::deallocateChunk(const vector<uint32_t> &v)
    {
        for (size_t i = 0; i < v.size(); ++i)
        {
            size_t iSize = (size_t)(((Block::tagChunkHead*)_pMap->getAbsolute(v[i]))->_iSize);
            _pChunkAllocator->deallocate((size_t)((v[i] >> 9) - 1), (size_t)(v[i] & 0x1FF));
            _pMap->delUsedDataMemSize(iSize);
            _pMap->delChunkCount();
        }
    }

    ///////////////////////////////////////////////////////////////////

    TC_Multi_HashMap_Malloc::HashMapLockItem::HashMapLockItem(TC_Multi_HashMap_Malloc *pMap, uint32_t iAddr)
        : _pMap(pMap)
        , _iAddr(iAddr)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapLockItem::HashMapLockItem(const HashMapLockItem &mcmdi)
        : _pMap(mcmdi._pMap)
        , _iAddr(mcmdi._iAddr)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapLockItem &TC_Multi_HashMap_Malloc::HashMapLockItem::operator=(const TC_Multi_HashMap_Malloc::HashMapLockItem &mcmdi)
    {
        if (this != &mcmdi)
        {
            _pMap = mcmdi._pMap;
            _iAddr = mcmdi._iAddr;
        }
        return (*this);
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockItem::operator==(const TC_Multi_HashMap_Malloc::HashMapLockItem &mcmdi)
    {
        return _pMap == mcmdi._pMap && _iAddr == mcmdi._iAddr;
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockItem::operator!=(const TC_Multi_HashMap_Malloc::HashMapLockItem &mcmdi)
    {
        return _pMap != mcmdi._pMap || _iAddr != mcmdi._iAddr;
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockItem::isDirty()
    {
        Block block(_pMap, _iAddr);
        return block.isDirty();
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockItem::isDelete()
    {
        Block block(_pMap, _iAddr);
        return block.isDelete();
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockItem::isOnlyKey()
    {
        Block block(_pMap, _iAddr);
        return block.isOnlyKey();
    }

    uint32_t TC_Multi_HashMap_Malloc::HashMapLockItem::getSyncTime()
    {
        Block block(_pMap, _iAddr);
        return block.getSyncTime();
    }

    uint32_t TC_Multi_HashMap_Malloc::HashMapLockItem::getExpireTime()
    {
        Block block(_pMap, _iAddr);
        return block.getExpireTime();
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::get(TC_Multi_HashMap_Malloc::Value &v)
    {
        Block block(_pMap, _iAddr);
        MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);

        // 先获取主key
        int ret = mainKey.get(v._mkey);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 获取数据
        BlockData data;
        ret = block.getBlockData(data);
        v.assign(data);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::getList(TC_Multi_HashMap_Malloc::Value &v)
    {
        Block block(_pMap, _iAddr);
        MainKey mainKey(_pMap, block.getListBlockHead()->_iMainKey);

        // 先获取主key
        int ret = mainKey.get(v._mkey);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 获取数据
        BlockData data;
        ret = block.getListBlockData(data);
        v.assign(data);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::getSet(TC_Multi_HashMap_Malloc::Value &v)
    {
        Block block(_pMap, _iAddr);
        MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);

        // 先获取主key
        int ret = mainKey.get(v._mkey);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 获取数据
        BlockData data;
        ret = block.getSetBlockData(data);
        v.assign(data);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::get(TC_Multi_HashMap_Malloc::PackValue& v)
    {
        Block block(_pMap, _iAddr);
        MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);

        // 先获取主key
        int ret = mainKey.get(v._mkey);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 获取数据
        BlockPackData data;
        ret = block.getBlockData(data);
        v.assign(data);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::get(string &mk, string &uk)
    {
        Block block(_pMap, _iAddr);
        MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);

        // 获取主key
        int ret = mainKey.get(mk);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 获取数据区
        string s;
        ret = block.get(s);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        try
        {
            // 提取除主key外的联合主键
            TC_PackOut po(s.c_str(), s.length());
            po >> uk;
        }
        catch (exception &ex)
        {
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::getSet(string &mk, string &v)
    {
        Block block(_pMap, _iAddr);
        MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);

        // 获取主key
        int ret = mainKey.get(mk);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 获取数据区
        ret = block.get(v);

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::getZSet(string &mk, string &v)
    {
        Block block(_pMap, _iAddr);
        MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);

        // 获取主key
        int ret = mainKey.getData(mk);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        // 获取数据区
        ret = block.getData(v);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::set(const string &mk, const string &uk, const string &v,
        uint32_t iExpireTime, uint8_t iVersion, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        Block block(_pMap, _iAddr);

        TC_PackIn pi;
        pi << uk;		// 数据区只存放uk，不存mk，节省空间
        pi << v;

        return block.set(pi.topacket().c_str(), pi.topacket().length(), false, iExpireTime, iVersion, vtData);
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::set(const string &mk, const string &uk, const string &v,
        uint32_t iExpireTime, uint8_t iVersion, bool bCheckExpire, uint32_t iNowTime, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        Block block(_pMap, _iAddr);

        TC_PackIn pi;
        pi << uk;		// 数据区只存放uk，不存mk，节省空间
        pi << v;

        return block.set(pi.topacket().c_str(), pi.topacket().length(), false, iExpireTime, iVersion, bCheckExpire, iNowTime, vtData);
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::set(const string &mk, const string &uk, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        Block block(_pMap, _iAddr);

        TC_PackIn pi;
        pi << uk;		// 数据区只存放uk

        return block.set(pi.topacket().c_str(), pi.topacket().length(), true, 0, 0, vtData);
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::setListBlock(const string &mk, const string &v, uint32_t iExpireTime, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        Block block(_pMap, _iAddr);

        if (iExpireTime != 0)
            block.getListBlockHead()->_iExpireTime = iExpireTime;

        return block.setList(v.c_str(), v.length(), vtData);
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::set(const string &mk, const string& v, uint64_t uniqueId, uint32_t iExpireTime, uint8_t iVersion, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        Block block(_pMap, _iAddr);

        TC_PackIn pi;
        pi << uniqueId;
        pi << v;

        return block.set(pi.topacket().c_str(), pi.topacket().length(), false, iExpireTime, iVersion, vtData);
    }

    int TC_Multi_HashMap_Malloc::HashMapLockItem::set(const string &mk, const string& v, uint32_t iExpireTime, uint8_t iVersion, vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        Block block(_pMap, _iAddr);

        return block.set(v.c_str(), v.length(), false, iExpireTime, iVersion, vtData);
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockItem::equal(const string &mk, const string &uk, TC_Multi_HashMap_Malloc::Value &v, int &ret)
    {
        ret = get(v);

        if ((ret == TC_Multi_HashMap_Malloc::RT_OK || ret == TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
            && mk == v._mkey && uk == v._ukey)
        {
            return true;
        }

        return false;
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockItem::equal(const string &mk, const string &uk, int &ret)
    {
        string mk1, uk1;
        ret = get(mk1, uk1);

        if (ret == TC_Multi_HashMap_Malloc::RT_OK && mk == mk1 && uk == uk1)
        {
            return true;
        }

        return false;
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockItem::equalSet(const string &mk, const string &v, int &ret)
    {
        string mk1, v1;
        ret = getSet(mk1, v1);

        if (ret == TC_Multi_HashMap_Malloc::RT_OK && mk == mk1 && v == v1)
        {
            return true;
        }

        return false;
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockItem::equalZSet(const string &mk, const string &v, int &ret)
    {
        string mk1, v1;
        ret = getZSet(mk1, v1);

        if (ret == TC_Multi_HashMap_Malloc::RT_OK && mk == mk1 && v == v1)
        {
            return true;
        }

        return false;
    }

    void TC_Multi_HashMap_Malloc::HashMapLockItem::nextItem(int iType)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);

        if (_iAddr == 0)
        {
            // 到头了
            return;
        }

        Block block(_pMap, _iAddr);

        if (iType == HashMapLockIterator::IT_BLOCK && keyType != MainKey::LIST_TYPE)
        {
            // 按联合主键索引下的block链遍历
            uint32_t index = block.getBlockHead()->_iIndex;

            //当前block链表有元素
            if (block.nextBlock())
            {
                _iAddr = block.getHead();
                return;
            }

            index += 1;

            while (index < _pMap->_hash.size())
            {
                //当前的hash桶也没有数据
                if (_pMap->item(index)->_iBlockAddr == 0)
                {
                    ++index;
                    continue;
                }

                _iAddr = _pMap->item(index)->_iBlockAddr;
                return;
            }

            _iAddr = 0;  //到尾部了
        }
        else if (iType == HashMapLockIterator::IT_SET && keyType != MainKey::LIST_TYPE)
        {
            // 按set链遍历
            _iAddr = block.getBlockHead()->_iSetNext;
        }
        else if (iType == HashMapLockIterator::IT_GET)
        {
            if (keyType != MainKey::LIST_TYPE)
            {
                // 按get(同一主key下的block)链遍历，即获取下一个get链上的数据
                _iAddr = block.getBlockHead()->_iMKBlockNext;
                if (_iAddr == 0)
                {
                    // 当前主key下的block链已经没有数据了，移动GET链上的下一个主key链
                    MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);
                    while (mainKey.getHeadPtr()->_iGetNext)
                    {
                        mainKey = MainKey(_pMap, mainKey.getHeadPtr()->_iGetNext);
                        _iAddr = mainKey.getHeadPtr()->_iBlockHead;
                        if (_iAddr != 0)
                        {
                            break;
                        }
                    }
                }
            }
            else
            {
                // 按get(同一主key下的block)链遍历，即获取下一个get链上的数据
                _iAddr = block.getListBlockHead()->_iMKBlockNext;
                if (_iAddr == 0)
                {
                    // 当前主key下的block链已经没有数据了，移动GET链上的下一个主key链
                    MainKey mainKey(_pMap, block.getListBlockHead()->_iMainKey);
                    while (mainKey.getHeadPtr()->_iGetNext)
                    {
                        mainKey = MainKey(_pMap, mainKey.getHeadPtr()->_iGetNext);
                        _iAddr = mainKey.getHeadPtr()->_iBlockHead;
                        if (_iAddr != 0)
                        {
                            break;
                        }
                    }
                }
            }
        }
        else if (iType == HashMapLockIterator::IT_MKEY)
        {
            if (keyType != MainKey::LIST_TYPE)
            {
                // 按同一主key下block链遍历
                _iAddr = block.getBlockHead()->_iMKBlockNext;
            }
            else
            {
                // 按同一主key下block链遍历
                _iAddr = block.getListBlockHead()->_iMKBlockNext;
            }
        }
        else if (iType == HashMapLockIterator::IT_UKEY && keyType != MainKey::LIST_TYPE)
        {
            // 按同一联合主键索引的block链遍历
            _iAddr = block.getBlockHead()->_iUKBlockNext;
        }
    }

    void TC_Multi_HashMap_Malloc::HashMapLockItem::prevItem(int iType)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);

        if (_iAddr == 0)
        {
            // 到头了
            return;
        }

        Block block(_pMap, _iAddr);

        if (iType == HashMapLockIterator::IT_BLOCK && keyType != MainKey::LIST_TYPE)
        {
            // 按联合主键索引下的block链遍历
            uint32_t index = block.getBlockHead()->_iIndex;
            if (block.prevBlock())
            {
                _iAddr = block.getHead();
                return;
            }

            while (index > 0)
            {
                //当前的hash桶也没有数据
                if (_pMap->item(index - 1)->_iBlockAddr == 0)
                {
                    --index;
                    continue;
                }

                //需要到这个桶的末尾
                Block tmp(_pMap, _pMap->item(index - 1)->_iBlockAddr);
                _iAddr = tmp.getLastBlockHead();

                return;
            }

            _iAddr = 0;  //到尾部了
        }
        else if (iType == HashMapLockIterator::IT_SET && keyType != MainKey::LIST_TYPE)
        {
            // 按set链遍历
            _iAddr = block.getBlockHead()->_iSetPrev;
        }
        else if (iType == HashMapLockIterator::IT_GET)
        {
            if (keyType != MainKey::LIST_TYPE)
            {
                // 按get(同一主key下的block)链遍历，即获取下一个get链上的数据
                _iAddr = block.getBlockHead()->_iMKBlockPrev;
                if (_iAddr == 0)
                {
                    // 当前主key下的block链已经没有数据了，移动get链上的上一个主key链
                    MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);
                    while (mainKey.getHeadPtr()->_iGetPrev)
                    {
                        mainKey = MainKey(_pMap, mainKey.getHeadPtr()->_iGetPrev);
                        _iAddr = mainKey.getHeadPtr()->_iBlockHead;
                        if (_iAddr != 0)
                        {
                            // 移动到主key链上的最后一个block
                            Block tmp(_pMap, _iAddr);
                            while (tmp.getBlockHead()->_iMKBlockNext)
                            {
                                _iAddr = tmp.getBlockHead()->_iMKBlockNext;
                                tmp = Block(_pMap, _iAddr);
                            }
                            break;
                        }
                    }
                }
            }
            else
            {
                // 按get(同一主key下的block)链遍历，即获取下一个get链上的数据
                _iAddr = block.getListBlockHead()->_iMKBlockPrev;
                if (_iAddr == 0)
                {
                    // 当前主key下的block链已经没有数据了，移动get链上的上一个主key链
                    MainKey mainKey(_pMap, block.getListBlockHead()->_iMainKey);
                    while (mainKey.getHeadPtr()->_iGetPrev)
                    {
                        mainKey = MainKey(_pMap, mainKey.getHeadPtr()->_iGetPrev);
                        _iAddr = mainKey.getHeadPtr()->_iBlockHead;
                        if (_iAddr != 0)
                        {
                            // 移动到主key链上的最后一个block
                            Block tmp(_pMap, _iAddr);
                            while (tmp.getListBlockHead()->_iMKBlockNext)
                            {
                                _iAddr = tmp.getListBlockHead()->_iMKBlockNext;
                                tmp = Block(_pMap, _iAddr);
                            }
                            break;
                        }
                    }
                }
            }
        }
        else if (iType == HashMapLockIterator::IT_MKEY)
        {
            if (keyType != MainKey::LIST_TYPE)
            {
                // 按同一主key下block链遍历
                _iAddr = block.getBlockHead()->_iMKBlockPrev;
            }
            else
            {
                // 按同一主key下block链遍历
                _iAddr = block.getListBlockHead()->_iMKBlockPrev;
            }
        }
        else if (iType == HashMapLockIterator::IT_UKEY && keyType != MainKey::LIST_TYPE)
        {
            // 按同一联合主键索引的block链遍历
            _iAddr = block.getBlockHead()->_iUKBlockPrev;
        }
    }

    ///////////////////////////////////////////////////////////////////

    TC_Multi_HashMap_Malloc::HashMapLockIterator::HashMapLockIterator()
        : _pMap(NULL), _iItem(NULL, 0)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapLockIterator::HashMapLockIterator(TC_Multi_HashMap_Malloc *pMap, uint32_t iAddr, int iType, int iOrder)
        : _pMap(pMap), _iItem(_pMap, iAddr), _iType(iType), _iOrder(iOrder)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapLockIterator::HashMapLockIterator(const HashMapLockIterator &it)
        : _pMap(it._pMap), _iItem(it._iItem), _iType(it._iType), _iOrder(it._iOrder)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapLockIterator& TC_Multi_HashMap_Malloc::HashMapLockIterator::operator=(const HashMapLockIterator &it)
    {
        if (this != &it)
        {
            _pMap = it._pMap;
            _iItem = it._iItem;
            _iType = it._iType;
            _iOrder = it._iOrder;
        }

        return (*this);
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockIterator::operator==(const HashMapLockIterator& mcmi)
    {
        if (_iItem.getAddr() != 0 || mcmi._iItem.getAddr() != 0)
        {
            return _pMap == mcmi._pMap
                && _iItem == mcmi._iItem
                && _iType == mcmi._iType
                && _iOrder == mcmi._iOrder;
        }

        return _pMap == mcmi._pMap;
    }

    bool TC_Multi_HashMap_Malloc::HashMapLockIterator::operator!=(const HashMapLockIterator& mcmi)
    {
        if (_iItem.getAddr() != 0 || mcmi._iItem.getAddr() != 0)
        {
            return _pMap != mcmi._pMap
                || _iItem != mcmi._iItem
                || _iType != mcmi._iType
                || _iOrder != mcmi._iOrder;
        }

        return _pMap != mcmi._pMap;
    }

    TC_Multi_HashMap_Malloc::HashMapLockIterator& TC_Multi_HashMap_Malloc::HashMapLockIterator::operator++()
    {
        if (_iOrder == IT_NEXT)
        {
            // 顺序遍历
            _iItem.nextItem(_iType);
        }
        else
        {
            // 逆序遍历
            _iItem.prevItem(_iType);
        }
        return (*this);

    }

    TC_Multi_HashMap_Malloc::HashMapLockIterator TC_Multi_HashMap_Malloc::HashMapLockIterator::operator++(int)
    {
        HashMapLockIterator it(*this);

        if (_iOrder == IT_NEXT)
        {
            // 顺序遍历
            _iItem.nextItem(_iType);
        }
        else
        {
            // 逆序遍历
            _iItem.prevItem(_iType);
        }

        return it;

    }

    ///////////////////////////////////////////////////////////////////

    TC_Multi_HashMap_Malloc::HashMapItem::HashMapItem(TC_Multi_HashMap_Malloc *pMap, uint32_t iIndex)
        : _pMap(pMap)
        , _iIndex(iIndex)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapItem::HashMapItem(const HashMapItem &mcmdi)
        : _pMap(mcmdi._pMap)
        , _iIndex(mcmdi._iIndex)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapItem &TC_Multi_HashMap_Malloc::HashMapItem::operator=(const TC_Multi_HashMap_Malloc::HashMapItem &mcmdi)
    {
        if (this != &mcmdi)
        {
            _pMap = mcmdi._pMap;
            _iIndex = mcmdi._iIndex;
        }
        return (*this);
    }

    bool TC_Multi_HashMap_Malloc::HashMapItem::operator==(const TC_Multi_HashMap_Malloc::HashMapItem &mcmdi)
    {
        return _pMap == mcmdi._pMap && _iIndex == mcmdi._iIndex;
    }

    bool TC_Multi_HashMap_Malloc::HashMapItem::operator!=(const TC_Multi_HashMap_Malloc::HashMapItem &mcmdi)
    {
        return _pMap != mcmdi._pMap || _iIndex != mcmdi._iIndex;
    }

    void TC_Multi_HashMap_Malloc::HashMapItem::get(vector<TC_Multi_HashMap_Malloc::Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);

        if (MainKey::LIST_TYPE == keyType)
            return;

        uint32_t iAddr = _pMap->item(_iIndex)->_iBlockAddr;

        while (iAddr != 0)
        {
            Block block(_pMap, iAddr);
            if (block.isDelete() == false)
            {
                MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);

                // 获取主key
                string mk;
                int ret;
                if (MainKey::ZSET_TYPE != keyType)
                    ret = mainKey.get(mk);
                else
                    ret = mainKey.getData(mk);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    // 获取联合主键及数据
                    BlockData blkData;
                    if (MainKey::HASH_TYPE == keyType)
                        ret = block.getBlockData(blkData);
                    else if (MainKey::SET_TYPE == keyType)
                        ret = block.getSetBlockData(blkData);
                    else if (MainKey::ZSET_TYPE == keyType)
                        ret = block.getZSetBlockData(blkData);

                    if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        TC_Multi_HashMap_Malloc::Value data(mk, blkData);
                        vtData.push_back(data);
                    }
                }
            }

            iAddr = block.getBlockHead()->_iUKBlockNext;
        }
    }

    void TC_Multi_HashMap_Malloc::HashMapItem::getExpireTime(vector<TC_Multi_HashMap_Malloc::ExpireTime> &vtTimes)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);

        uint32_t iAddr = _pMap->item(_iIndex)->_iBlockAddr;

        while (iAddr != 0)
        {
            Block block(_pMap, iAddr);

            //已经标记为删除的就不获取了
            if (MainKey::LIST_TYPE != keyType && block.isDelete() == false)
            {
                MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);
                TC_Multi_HashMap_Malloc::ExpireTime tmExpire;

                // 获取主key
                int ret;
                if (MainKey::ZSET_TYPE != keyType)
                    ret = mainKey.get(tmExpire._mkey);
                else
                    ret = mainKey.getData(tmExpire._mkey);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    // 获取联合主键及过期时间
                    if (MainKey::LIST_TYPE != keyType)
                        tmExpire._iExpireTime = block.getExpireTime();
                    else
                        tmExpire._iExpireTime = block.getListExpireTime();
                    if (MainKey::HASH_TYPE == keyType)
                    {
                        // 直接解码联合主键，比解码全部数据快
                        string s;
                        ret = block.get(s);
                        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            try
                            {
                                // 提取除主key外的联合主键
                                TC_PackOut po(s.c_str(), s.length());
                                po >> tmExpire._ukey;
                                vtTimes.push_back(tmExpire);
                            }
                            catch (exception &ex)
                            {
                            }
                        }
                    }
                    else if (MainKey::ZSET_TYPE == keyType)
                    {
                        block.getData(tmExpire._ukey);
                    }
                    else if (MainKey::SET_TYPE == keyType)
                    {
                        block.get(tmExpire._ukey);
                    }
                    else if (MainKey::LIST_TYPE == keyType)
                    {
                        block.getList(tmExpire._ukey);
                    }
                }
            }

            iAddr = block.getBlockHead()->_iUKBlockNext;
        }
    }

    void TC_Multi_HashMap_Malloc::HashMapItem::eraseExpireData(time_t now)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);
        if (MainKey::LIST_TYPE == keyType)
            return;

        TC_Multi_HashMap_Malloc::FailureRecover recover(_pMap);

        uint32_t iAddr = _pMap->item(_iIndex)->_iBlockAddr;

        while (iAddr != 0)
        {
            Block block(_pMap, iAddr);
            iAddr = block.getBlockHead()->_iUKBlockNext;

            //已经标记为删除的就不获取了
            MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);

            // 获取联合主键及过期时间
            if (block.getExpireTime() != 0 && block.getExpireTime() <= (uint32_t)now)
            {
                if (MainKey::HASH_TYPE == keyType || MainKey::SET_TYPE == keyType)
                {
                    block.erase(true);
                }
                else if (MainKey::ZSET_TYPE == keyType)
                {
                    //_pMap->delSkipList(mainKey, block.getHead());
                    block.eraseZSet(true);
                }
            }
        }
    }

    void TC_Multi_HashMap_Malloc::HashMapItem::getDeleteData(vector<TC_Multi_HashMap_Malloc::DeleteData> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);
        if (MainKey::LIST_TYPE == keyType)
            return;

        uint32_t iAddr = _pMap->item(_iIndex)->_iBlockAddr;

        while (iAddr != 0)
        {
            Block block(_pMap, iAddr);

            //获取已经标记为删除的数据
            if (block.isDelete() == true)
            {
                MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);
                TC_Multi_HashMap_Malloc::DeleteData tmDel;

                // 获取主key
                int ret;
                if (MainKey::ZSET_TYPE == keyType)
                    ret = mainKey.getData(tmDel._mkey);
                else
                    ret = mainKey.get(tmDel._mkey);
                if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                {
                    // 获取删除时间
                    tmDel._iDeleteTime = block.getSyncTime();

                    if (MainKey::HASH_TYPE == keyType)
                    {
                        // 直接解码联合主键，比解码全部数据快
                        string s;
                        ret = block.get(s);
                        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            try
                            {
                                // 提取除主key外的联合主键
                                TC_PackOut po(s.c_str(), s.length());
                                po >> tmDel._ukey;
                            }
                            catch (exception &ex)
                            {
                            }
                        }
                    }
                    else if (MainKey::SET_TYPE == keyType)
                    {
                        block.get(tmDel._ukey);
                    }
                    else
                    {
                        block.getData(tmDel._ukey);
                    }

                    vtData.push_back(tmDel);
                }
            }

            iAddr = block.getBlockHead()->_iUKBlockNext;
        }
    }

    void TC_Multi_HashMap_Malloc::HashMapItem::nextItem()
    {
        if (_iIndex == (uint32_t)(-1))
        {
            return;
        }

        if (_iIndex >= _pMap->getHashCount() - 1)
        {
            _iIndex = (uint32_t)(-1);
            return;
        }
        _iIndex++;
    }

    int TC_Multi_HashMap_Malloc::HashMapItem::setDirty()
    {
        if (_pMap->getMapHead()._bReadOnly) return RT_READONLY;

        uint32_t iAddr = _pMap->item(_iIndex)->_iBlockAddr;

        while (iAddr != 0)
        {
            Block block(_pMap, iAddr);
            if (block.isDelete() == false)
            {
                if (!block.isOnlyKey())
                {
                    TC_Multi_HashMap_Malloc::FailureRecover recover(_pMap);

                    block.setDirty(true);
                    block.refreshSetList();
                }
            }

            iAddr = block.getBlockHead()->_iUKBlockNext;
        }

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::HashMapItem::setClean()
    {
        if (_pMap->getMapHead()._bReadOnly) return RT_READONLY;

        uint32_t iAddr = _pMap->item(_iIndex)->_iBlockAddr;

        while (iAddr != 0)
        {
            Block block(_pMap, iAddr);
            if (block.isDelete() == false)
            {
                if (!block.isOnlyKey())
                {
                    TC_Multi_HashMap_Malloc::FailureRecover recover(_pMap);

                    block.setDirty(false);
                    block.refreshSetList();
                }
            }

            iAddr = block.getBlockHead()->_iUKBlockNext;
        }

        return RT_OK;
    }

    ///////////////////////////////////////////////////////////////////

    TC_Multi_HashMap_Malloc::HashMapIterator::HashMapIterator()
        : _pMap(NULL), _iItem(NULL, (uint32_t)-1)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapIterator::HashMapIterator(TC_Multi_HashMap_Malloc *pMap, uint32_t iIndex)
        : _pMap(pMap), _iItem(_pMap, iIndex)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapIterator::HashMapIterator(const HashMapIterator &it)
        : _pMap(it._pMap), _iItem(it._iItem)
    {
    }

    TC_Multi_HashMap_Malloc::HashMapIterator& TC_Multi_HashMap_Malloc::HashMapIterator::operator=(const HashMapIterator &it)
    {
        if (this != &it)
        {
            _pMap = it._pMap;
            _iItem = it._iItem;
        }

        return (*this);
    }

    bool TC_Multi_HashMap_Malloc::HashMapIterator::operator==(const HashMapIterator& mcmi)
    {
        if (_iItem.getIndex() != (uint32_t)-1 || mcmi._iItem.getIndex() != (uint32_t)-1)
        {
            return _pMap == mcmi._pMap && _iItem == mcmi._iItem;
        }

        return _pMap == mcmi._pMap;
    }

    bool TC_Multi_HashMap_Malloc::HashMapIterator::operator!=(const HashMapIterator& mcmi)
    {
        if (_iItem.getIndex() != (uint32_t)-1 || mcmi._iItem.getIndex() != (uint32_t)-1)
        {
            return _pMap != mcmi._pMap || _iItem != mcmi._iItem;
        }

        return _pMap != mcmi._pMap;
    }

    TC_Multi_HashMap_Malloc::HashMapIterator& TC_Multi_HashMap_Malloc::HashMapIterator::operator++()
    {
        _iItem.nextItem();
        return (*this);
    }

    TC_Multi_HashMap_Malloc::HashMapIterator TC_Multi_HashMap_Malloc::HashMapIterator::operator++(int)
    {
        HashMapIterator it(*this);
        _iItem.nextItem();
        return it;
    }

    //////////////////////////////////////////////////////////////////////////////////
    TC_Multi_HashMap_Malloc::MKHashMapItem::MKHashMapItem(TC_Multi_HashMap_Malloc *pMap, uint32_t iIndex)
        : _pMap(pMap)
        , _iIndex(iIndex)
    {
    }

    TC_Multi_HashMap_Malloc::MKHashMapItem::MKHashMapItem(const MKHashMapItem &mcmdi)
        : _pMap(mcmdi._pMap)
        , _iIndex(mcmdi._iIndex)
    {
    }

    TC_Multi_HashMap_Malloc::MKHashMapItem &TC_Multi_HashMap_Malloc::MKHashMapItem::operator=(const TC_Multi_HashMap_Malloc::MKHashMapItem &mcmdi)
    {
        if (this != &mcmdi)
        {
            _pMap = mcmdi._pMap;
            _iIndex = mcmdi._iIndex;
        }
        return (*this);
    }

    bool TC_Multi_HashMap_Malloc::MKHashMapItem::operator==(const TC_Multi_HashMap_Malloc::MKHashMapItem &mcmdi)
    {
        return _pMap == mcmdi._pMap && _iIndex == mcmdi._iIndex;
    }

    bool TC_Multi_HashMap_Malloc::MKHashMapItem::operator!=(const TC_Multi_HashMap_Malloc::MKHashMapItem &mcmdi)
    {
        return _pMap != mcmdi._pMap || _iIndex != mcmdi._iIndex;
    }
    void TC_Multi_HashMap_Malloc::MKHashMapItem::get(map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > > &mData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);
        uint32_t iMainkeyAddr = _pMap->itemMainKey(_iIndex)->_iMainKeyAddr;

        while (iMainkeyAddr != 0)
        {
            MainKey mainKey(_pMap, iMainkeyAddr);
            //MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);
            uint32_t iBlockAddr;;
            // 获取主key
            string mk;
            int ret;
            if (MainKey::ZSET_TYPE != keyType)
            {
                iBlockAddr = mainKey.getHeadPtr()->_iBlockHead;
                ret = mainKey.get(mk);
            }
            else
            {
                iBlockAddr = mainKey.getNext(0);
                ret = mainKey.getData(mk);
            }
            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                while (iBlockAddr != 0)
                {
                    // 获取联合主键及数据
                    Block block(_pMap, iBlockAddr);
                    if (MainKey::HASH_TYPE == keyType)
                    {
                        if (block.isDelete() == false)
                        {
                            BlockData blkData;
                            ret = block.getBlockData(blkData);
                            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                TC_Multi_HashMap_Malloc::Value data(mk, blkData);
                                map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator it = mData.find(mk);
                                if (it != mData.end())
                                {
                                    mData[mk].second.push_back(data);
                                }
                                else
                                {
                                    vector<TC_Multi_HashMap_Malloc::Value> tmpVec;
                                    bool isFull = mainKey.ISFULLDATA();
                                    pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > tmpPair(isFull, tmpVec);
                                    mData.insert(pair<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >(mk, tmpPair));
                                    mData[mk].second.push_back(data);
                                }
                            }
                        }
                        iBlockAddr = block.getBlockHead()->_iMKBlockNext;
                    }
                    else if (MainKey::LIST_TYPE == keyType)
                    {
                        BlockData blkData;
                        ret = block.getListBlockData(blkData);
                        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            TC_Multi_HashMap_Malloc::Value data(mk, blkData);
                            map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator it = mData.find(mk);
                            if (it != mData.end())
                            {
                                mData[mk].second.push_back(data);
                            }
                            else
                            {
                                vector<TC_Multi_HashMap_Malloc::Value> tmpVec;
                                bool isFull = mainKey.ISFULLDATA();
                                pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > tmpPair(isFull, tmpVec);
                                mData.insert(pair<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >(mk, tmpPair));
                                mData[mk].second.push_back(data);
                            }
                        }
                        iBlockAddr = block.getListBlockHead()->_iMKBlockNext;
                    }
                    else if (MainKey::SET_TYPE == keyType)
                    {
                        BlockData blkData;
                        ret = block.getSetBlockData(blkData);
                        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            TC_Multi_HashMap_Malloc::Value data(mk, blkData);
                            map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator it = mData.find(mk);
                            if (it != mData.end())
                            {
                                mData[mk].second.push_back(data);
                            }
                            else
                            {
                                vector<TC_Multi_HashMap_Malloc::Value> tmpVec;
                                bool isFull = mainKey.ISFULLDATA();
                                pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > tmpPair(isFull, tmpVec);
                                mData.insert(pair<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >(mk, tmpPair));
                                mData[mk].second.push_back(data);
                            }
                        }
                        iBlockAddr = block.getBlockHead()->_iMKBlockNext;
                    }
                    else if (MainKey::ZSET_TYPE == keyType)
                    {
                        BlockData blkData;
                        ret = block.getZSetBlockData(blkData);
                        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            TC_Multi_HashMap_Malloc::Value data(mk, blkData);
                            map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator it = mData.find(mk);
                            if (it != mData.end())
                            {
                                mData[mk].second.push_back(data);
                            }
                            else
                            {
                                vector<TC_Multi_HashMap_Malloc::Value> tmpVec;
                                bool isFull = mainKey.ISFULLDATA();
                                pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > tmpPair(isFull, tmpVec);
                                mData.insert(pair<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >(mk, tmpPair));
                                mData[mk].second.push_back(data);
                            }
                        }
                        iBlockAddr = block.getNext(0);
                    }
                }
            }
            iMainkeyAddr = mainKey.getHeadPtr()->_iNext;
        }
    }
    void TC_Multi_HashMap_Malloc::MKHashMapItem::getAllData(map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > > &mData)
    {
        uint32_t iMainkeyAddr = _pMap->itemMainKey(_iIndex)->_iMainKeyAddr;

        while (iMainkeyAddr != 0)
        {
            MainKey mainKey(_pMap, iMainkeyAddr);
            //MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);
            uint32_t iBlockAddr = mainKey.getHeadPtr()->_iBlockHead;
            // 获取主key
            string mk;
            int ret;
            if (_pMap->_pHead->_iKeyType != MainKey::ZSET_TYPE)
                ret = mainKey.get(mk);
            else
            {
                iBlockAddr = mainKey.getNext(0);
                ret = mainKey.getData(mk);
            }

            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                if (iBlockAddr == 0)
                {
                    TC_Multi_HashMap_Malloc::Value data;
                    map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator it = mData.find(mk);
                    if (it != mData.end())
                    {
                        mData[mk].second.push_back(data);
                    }
                    else
                    {
                        vector<TC_Multi_HashMap_Malloc::Value> tmpVec;
                        pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > tmpPair(true, tmpVec);
                        mData.insert(pair<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >(mk, tmpPair));
                        mData[mk].second.push_back(data);
                    }
                }

                if (_pMap->_pHead->_iKeyType == MainKey::HASH_TYPE)
                {
                    while (iBlockAddr != 0)
                    {
                        // 获取联合主键及数据
                        Block block(_pMap, iBlockAddr);
                        if (block.isDelete() == false)
                        {
                            BlockData blkData;
                            ret = block.getBlockData(blkData);
                            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                TC_Multi_HashMap_Malloc::Value data(mk, blkData);
                                map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator it = mData.find(mk);
                                if (it != mData.end())
                                {
                                    mData[mk].second.push_back(data);
                                }
                                else
                                {
                                    vector<TC_Multi_HashMap_Malloc::Value> tmpVec;
                                    bool isFull = mainKey.ISFULLDATA();
                                    pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > tmpPair(isFull, tmpVec);
                                    mData.insert(pair<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >(mk, tmpPair));
                                    mData[mk].second.push_back(data);
                                }
                            }
                        }
                        iBlockAddr = block.getBlockHead()->_iMKBlockNext;
                    }
                }
                else if (_pMap->_pHead->_iKeyType == MainKey::LIST_TYPE)
                {
                    while (iBlockAddr != 0)
                    {
                        // 获取联合主键及数据
                        Block block(_pMap, iBlockAddr);

                        BlockData blkData;
                        ret = block.getListBlockData(blkData);
                        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                        {
                            TC_Multi_HashMap_Malloc::Value data(mk, blkData);
                            map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator it = mData.find(mk);
                            if (it != mData.end())
                            {
                                mData[mk].second.push_back(data);
                            }
                            else
                            {
                                vector<TC_Multi_HashMap_Malloc::Value> tmpVec;
                                pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > tmpPair(true, tmpVec);
                                mData.insert(pair<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >(mk, tmpPair));
                                mData[mk].second.push_back(data);
                            }
                        }
                        iBlockAddr = block.getListBlockHead()->_iMKBlockNext;
                    }
                }
                else if (_pMap->_pHead->_iKeyType == MainKey::SET_TYPE)
                {
                    while (iBlockAddr != 0)
                    {
                        // 获取联合主键及数据
                        Block block(_pMap, iBlockAddr);
                        if (block.isDelete() == false)
                        {
                            BlockData blkData;
                            ret = block.getSetBlockData(blkData);
                            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                TC_Multi_HashMap_Malloc::Value data(mk, blkData);
                                map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator it = mData.find(mk);
                                if (it != mData.end())
                                {
                                    mData[mk].second.push_back(data);
                                }
                                else
                                {
                                    vector<TC_Multi_HashMap_Malloc::Value> tmpVec;
                                    pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > tmpPair(true, tmpVec);
                                    mData.insert(pair<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >(mk, tmpPair));
                                    mData[mk].second.push_back(data);
                                }
                            }
                        }
                        iBlockAddr = block.getBlockHead()->_iMKBlockNext;
                    }
                }
                else if (_pMap->_pHead->_iKeyType == MainKey::ZSET_TYPE)
                {
                    while (iBlockAddr != 0)
                    {
                        // 获取联合主键及数据
                        Block block(_pMap, iBlockAddr);
                        if (block.isDelete() == false)
                        {
                            BlockData blkData;
                            ret = block.getZSetBlockData(blkData);
                            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                TC_Multi_HashMap_Malloc::Value data(mk, blkData);
                                map<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >::iterator it = mData.find(mk);
                                if (it != mData.end())
                                {
                                    mData[mk].second.push_back(data);
                                }
                                else
                                {
                                    vector<TC_Multi_HashMap_Malloc::Value> tmpVec;
                                    pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > tmpPair(true, tmpVec);
                                    mData.insert(pair<string, pair<bool, vector<TC_Multi_HashMap_Malloc::Value> > >(mk, tmpPair));
                                    mData[mk].second.push_back(data);
                                }
                            }
                        }
                        iBlockAddr = block.getNext(0);
                    }
                }
            }
            iMainkeyAddr = mainKey.getHeadPtr()->_iNext;
        }
    }

    void TC_Multi_HashMap_Malloc::MKHashMapItem::eraseExpireData(time_t now)
    {
        if (_pMap->_pHead->_iKeyType != MainKey::LIST_TYPE)
            return;

        uint32_t iMainkeyAddr = _pMap->itemMainKey(_iIndex)->_iMainKeyAddr;

        while (iMainkeyAddr != 0)
        {
            MainKey mainKey(_pMap, iMainkeyAddr);
            iMainkeyAddr = mainKey.getHeadPtr()->_iNext;
            //MainKey mainKey(_pMap, block.getBlockHead()->_iMainKey);
            uint32_t iBlockAddr = mainKey.getHeadPtr()->_iBlockHead;

            while (iBlockAddr != 0)
            {
                // 获取联合主键及数据
                Block block(_pMap, iBlockAddr);
                iBlockAddr = block.getListBlockHead()->_iMKBlockNext;

                if (block.getListBlockHead()->_iExpireTime != 0 && block.getListBlockHead()->_iExpireTime <= (uint32_t)now)
                {
                    block.eraseList(true);
                }
            }
        }
    }

    void TC_Multi_HashMap_Malloc::MKHashMapItem::getStaticData(size_t &iDirty, size_t &iDel)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return;

        uint32_t iMainkeyAddr = _pMap->itemMainKey(_iIndex)->_iMainKeyAddr;

        while (iMainkeyAddr != 0)
        {
            MainKey mainKey(_pMap, iMainkeyAddr);
            uint32_t iBlockAddr = mainKey.getHeadPtr()->_iBlockHead;

            bool bDirty = false;
            while (iBlockAddr != 0)
            {
                Block block(_pMap, iBlockAddr);

                if (block.isDelete())
                    iDel++;

                if (block.isDirty())
                    bDirty = true;

                iBlockAddr = block.getBlockHead()->_iMKBlockNext;
            }

            if (bDirty)
                iDirty += mainKey.getHeadPtr()->_iBlockCount;
            iMainkeyAddr = mainKey.getHeadPtr()->_iNext;
        }
    }

    void TC_Multi_HashMap_Malloc::MKHashMapItem::getKey(vector<string> &mData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);
        uint32_t iMainkeyAddr = _pMap->itemMainKey(_iIndex)->_iMainKeyAddr;

        while (iMainkeyAddr != 0)
        {
            MainKey mainKey(_pMap, iMainkeyAddr);
            // 获取主key
            string mk;
            int ret;
            if (MainKey::ZSET_TYPE != keyType)
                ret = mainKey.get(mk);
            else
                ret = mainKey.getData(mk);
            if (ret == TC_Multi_HashMap_Malloc::RT_OK)
            {
                mData.push_back(mk);
            }
            iMainkeyAddr = mainKey.getHeadPtr()->_iNext;
        }
    }
    void TC_Multi_HashMap_Malloc::MKHashMapItem::getExpireTime(map<string, vector<TC_Multi_HashMap_Malloc::ExpireTime> > &mTimes)
    {}
    bool TC_Multi_HashMap_Malloc::MKHashMapItem::setIndex(uint32_t index)
    {
        if (index > _pMap->getMainKeyHashCount() - 1)
            return false;

        _iIndex = index;
        return true;
    }
    void TC_Multi_HashMap_Malloc::MKHashMapItem::nextItem()
    {
        if (_iIndex == (uint32_t)(-1))
        {
            return;
        }

        if (_iIndex >= _pMap->getMainKeyHashCount() - 1)
        {
            _iIndex = (uint32_t)(-1);
            return;
        }
        _iIndex++;
    }
    int TC_Multi_HashMap_Malloc::MKHashMapItem::delOnlyKey()
    {
        if (_pMap->getMapHead()._bReadOnly) return RT_READONLY;

        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pMap->_pHead->_iKeyType);

        uint32_t iAddr = _pMap->itemMainKey(_iIndex)->_iMainKeyAddr;

        while (iAddr != 0)
        {
            MainKey MKblock(_pMap, iAddr);
            iAddr = MKblock.getHeadPtr()->_iNext;
            if (MainKey::ZSET_TYPE != keyType)
            {
                if (MKblock.getHeadPtr()->_iBlockHead == 0)
                {
                    TC_Multi_HashMap_Malloc::FailureRecover recover(_pMap);
                    vector<Value> vtData;
                    MKblock.erase(vtData);
                }
            }
            else
            {
                if (MKblock.getNext(0) == 0)
                {
                    TC_Multi_HashMap_Malloc::FailureRecover recover(_pMap);
                    vector<Value> vtData;
                    MKblock.erase(vtData);
                }
            }
        }
        return RT_OK;
    }

    TC_Multi_HashMap_Malloc::MKHashMapIterator::MKHashMapIterator()
        : _pMap(NULL), _iItem(NULL, (uint32_t)-1)
    {
    }

    TC_Multi_HashMap_Malloc::MKHashMapIterator::MKHashMapIterator(TC_Multi_HashMap_Malloc *pMap, uint32_t iIndex)
        : _pMap(pMap), _iItem(_pMap, iIndex)
    {

    }

    TC_Multi_HashMap_Malloc::MKHashMapIterator::MKHashMapIterator(const MKHashMapIterator &it)
        : _pMap(it._pMap), _iItem(it._iItem)
    {
    }

    TC_Multi_HashMap_Malloc::MKHashMapIterator& TC_Multi_HashMap_Malloc::MKHashMapIterator::operator=(const MKHashMapIterator &it)
    {
        if (this != &it)
        {
            _pMap = it._pMap;
            _iItem = it._iItem;
        }

        return (*this);
    }

    bool TC_Multi_HashMap_Malloc::MKHashMapIterator::operator==(const MKHashMapIterator& mcmi)
    {
        if (_iItem.getIndex() != (uint32_t)-1 || mcmi._iItem.getIndex() != (uint32_t)-1)
        {
            return _pMap == mcmi._pMap && _iItem == mcmi._iItem;
        }

        return _pMap == mcmi._pMap;
    }

    bool TC_Multi_HashMap_Malloc::MKHashMapIterator::operator!=(const MKHashMapIterator& mcmi)
    {
        if (_iItem.getIndex() != (uint32_t)-1 || mcmi._iItem.getIndex() != (uint32_t)-1)
        {
            return _pMap != mcmi._pMap || _iItem != mcmi._iItem;
        }

        return _pMap != mcmi._pMap;
    }

    TC_Multi_HashMap_Malloc::MKHashMapIterator& TC_Multi_HashMap_Malloc::MKHashMapIterator::operator++()
    {
        _iItem.nextItem();
        return (*this);
    }

    TC_Multi_HashMap_Malloc::MKHashMapIterator TC_Multi_HashMap_Malloc::MKHashMapIterator::operator++(int)
    {
        MKHashMapIterator it(*this);
        _iItem.nextItem();
        return it;
    }
    ////////////////

    void TC_Multi_HashMap_Malloc::init(void *pAddr)
    {
        _pHead = static_cast<tagMapHead*>(pAddr);
        _pstInnerModify = static_cast<tagModifyHead*>((void*)((char*)pAddr + sizeof(tagMapHead)));
        _pstOuterModify = static_cast<tagModifyHead*>((void*)((char*)pAddr + sizeof(tagMapHead) + sizeof(tagModifyHead)));
        _iRefCount = 0;
    }

    void TC_Multi_HashMap_Malloc::initMainKeySize(size_t iMainKeySize)
    {
        _iMainKeySize = iMainKeySize;
    }

    void TC_Multi_HashMap_Malloc::initDataSize(size_t iDataSize)
    {
        _iDataSize = iDataSize;
    }

    void TC_Multi_HashMap_Malloc::create(void *pAddr, size_t iSize, uint8_t _iKeyType)
    {
        if (sizeof(tagHashItem)
            + sizeof(tagMainKeyHashItem)
            + sizeof(tagMapHead)
            + sizeof(tagModifyHead) * 2
            + sizeof(TC_MallocChunkAllocator::tagChunkAllocatorHead)
            + 10 > iSize)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::create] mem size not enougth.");
        }

        if (_iDataSize == 0)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::create] init data size error:" + TC_Common::tostr(_iDataSize));
        }

        init(pAddr);

        _rand.seed_ = 0xdeadbeaf & 0x7fffffffu;

        _pHead->_bInit = false;
        _pHead->_cMaxVersion = MAX_VERSION;
        _pHead->_cMinVersion = MIN_VERSION;
        _pHead->_bReadOnly = false;
        _pHead->_bAutoErase = true;
        _pHead->_cEraseMode = ERASEBYGET;
        _pHead->_iMemSize = iSize;
        _pHead->_iMainKeySize = _iMainKeySize;
        _pHead->_iDataSize = _iDataSize;
        _pHead->_iMinChunkSize = _iMinChunkSize;
        _pHead->_fHashRatio = _fHashRatio;
        _pHead->_fMainKeyRatio = _fMainKeyRatio;
        _pHead->_iElementCount = 0;
        _pHead->_iMainKeyCount = 0;
        _pHead->_iEraseCount = 10;
        _pHead->_iSetHead = 0;
        _pHead->_iSetTail = 0;
        _pHead->_iGetHead = 0;
        _pHead->_iGetTail = 0;
        _pHead->_iDirtyCount = 0;
        _pHead->_iDirtyTail = 0;
        _pHead->_iSyncTime = 60 * 10;  //缺省十分钟回写一次
        _pHead->_iDataUsedChunk = 0;
        _pHead->_iUsedDataMem = 0;
        _pHead->_iMKUsedChunk = 0;
        _pHead->_iMKUsedMem = 0;
        _pHead->_iGetCount = 0;
        _pHead->_iHitCount = 0;
        _pHead->_iBackupTail = 0;
        _pHead->_iSyncTail = 0;
        _pHead->_iMKOnlyKeyCount = 0;
        _pHead->_iOnlyKeyCount = 0;
        _pHead->_iMaxBlockCount = 0;
        _pHead->_iMaxLevel = 12;
        _pHead->_iKeyType = _iKeyType;
        memset(_pHead->_cReserve, 0, sizeof(_pHead->_cReserve));

        _prev = new uint32_t[_pHead->_iMaxLevel];

        _pstInnerModify->_cModifyStatus = 0;
        _pstInnerModify->_iNowIndex = 0;

        _pstOuterModify->_cModifyStatus = 0;
        _pstOuterModify->_iNowIndex = 0;

        _pstCurrModify = _pstOuterModify;

        size_t iMainKeyChunkSize = 0;
        if (_iMainKeySize > 0)
        {
            // 外部有指定主key大小，将单独为主key分配内存
            iMainKeyChunkSize = (sizeof(MainKey::tagMainKeyHead) + _iMainKeySize > _iMinChunkSize) ? (sizeof(MainKey::tagMainKeyHead) + _iMainKeySize) : _iMinChunkSize;

            if (_pMainKeyAllocator == NULL)
            {
                _pMainKeyAllocator = new BlockAllocator(this);
            }
        }
        else
        {
            // 外部没有指定主key大小，主key与数据区使用相同的内存分配器
            _pMainKeyAllocator = _pDataAllocator;
        }

        size_t iBlockHeadSize = sizeof(Block::tagBlockHead);
        // 计算数据区平均Chunk大小
        size_t iDataChunkSize = (_iDataSize + iBlockHeadSize > _iMinChunkSize) ? (_iDataSize + iBlockHeadSize) : _iMinChunkSize;

        /**
        * 内存计算公式
        * 假设主key hash个数为x，联合hash个数为y，主key大小为m，数据区大小为n，
        * hash比率(hash比率是指数据记录数与hash索引数的比例)为r1，主key记录数与hash索引数共享这个比率
        * 联合主键与主key的比率为r2，主key hash项的大小为h1，联合hash项的大小为h2，总内存空间大小为t
        * 则计算公式为：
        * x*h1 + y*h2 + x*m*r1 + y*n*r1 = t  -- 即 总内存大小 = 主key hash区大小 + 联合主键hash区大小 + 主key区大小 + 数据区大小
        * y = x*r2
        * 则 x = t/(h1+h2*r2+m*r1+n*r1*r2)
        */

        // 计算近似主key Hash个数
        size_t iMHashCount = (iSize - sizeof(tagMapHead) - sizeof(tagModifyHead) * 2 - sizeof(TC_MallocChunkAllocator::tagChunkAllocatorHead)) /
            (sizeof(tagMainKeyHashItem) +
            (size_t)(sizeof(tagHashItem)*_fMainKeyRatio) +
                (size_t)(iMainKeyChunkSize*_fHashRatio) +
                (size_t)(iDataChunkSize*_fHashRatio*_fMainKeyRatio));
        // 采用最近的素数作为hash值
        iMHashCount = getMinPrimeNumber(iMHashCount);
        // 联合hash个数
        size_t iUHashCount = getMinPrimeNumber(size_t(iMHashCount * _fMainKeyRatio));

        void *pHashAddr = (char*)_pHead + sizeof(tagMapHead) + sizeof(tagModifyHead) * 2;

        // 主key hash索引区
        size_t iHashMemSize = TC_MemVector<tagMainKeyHashItem>::calcMemSize(iMHashCount);
        if ((char*)pHashAddr - (char*)_pHead + iHashMemSize > iSize)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::create] mem size not enougth.");
        }
        _hashMainKey.create(pHashAddr, iHashMemSize);
        pHashAddr = (char*)pHashAddr + _hashMainKey.getMemSize();

        // 联合hash索引区
        iHashMemSize = TC_MemVector<tagHashItem>::calcMemSize(iUHashCount);
        if ((char*)pHashAddr - (char*)_pHead + iHashMemSize > iSize)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::create] mem size not enougth.");
        }
        _hash.create(pHashAddr, iHashMemSize);

        // 主key chunk区
        void *pDataAddr = (char*)pHashAddr + _hash.getMemSize();
        if (_iMainKeySize > 0)
        {
            size_t iMainKeyTotalSize = (size_t)(iMHashCount * _fHashRatio * iMainKeyChunkSize);
            if ((char*)pDataAddr - (char*)_pHead + iMainKeyTotalSize > iSize)
            {
                throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::create] mem size not enougth.");
            }
            _pMainKeyAllocator->create(pDataAddr, iMainKeyTotalSize);

            if ((uint64_t)_pMainKeyAllocator->getAllCapacity() > ((uint64_t)((uint32_t)(-1) >> 9)*(1 << 9)*_iMinChunkSize))
            {
                throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::create] mem size too large");
            }

            pDataAddr = (char*)pDataAddr + _pMainKeyAllocator->getMemSize();
        }

        // chunk数据区
        if ((char*)pDataAddr - (char*)_pHead + _pHead->_iMinChunkSize > iSize)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::create] mem size not enougth.");
        }
        _pDataAllocator->create(pDataAddr, iSize - ((char*)pDataAddr - (char*)_pHead));

        if ((uint64_t)_pDataAllocator->getAllCapacity() > ((uint64_t)((uint32_t)(-1) >> 9)*(1 << 9)*_iMinChunkSize))
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::create] mem size too large");
        }

        _tmpBufSize = 1024 * 1024 * 2;
        _tmpBuf = new char[_tmpBufSize];

        _iReadP = 0;
        _iReadPBak = 0;

        _pHead->_bInit = true;
    }

    void TC_Multi_HashMap_Malloc::connect(void *pAddr, size_t iSize, uint8_t _iKeyType)
    {
        init(pAddr);

        _rand.seed_ = 0xdeadbeaf & 0x7fffffffu;

        if (!_pHead->_bInit)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::connect] hash map created unsuccessfully, so can not be connected");
        }

        /*if(_pHead->_cMaxVersion != MAX_VERSION || _pHead->_cMinVersion != MIN_VERSION)
        {
            // 版本不匹配
            ostringstream os;
            os << (int)_pHead->_cMaxVersion << "." << (int)_pHead->_cMinVersion << " != " << ((int)MAX_VERSION) << "." << ((int)MIN_VERSION);

            if (_pHead->_cMaxVersion == MAX_VERSION && _pHead->_cMinVersion != MIN_VERSION && _pHead->_iKeyType == MainKey::ZSET_TYPE)
            {
                throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::connect] hash map version not equal:" + os.str() + " (data != code)");
            }
            else if (_pHead->_cMaxVersion == MAX_VERSION && _pHead->_cMinVersion != MIN_VERSION)
            {
            }
            else
                throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::connect] hash map version not equal:" + os.str() + " (data != code)");
        }*/

        if (_pHead->_iMemSize != iSize)
        {
            // 内存大小不匹配
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::connect] hash map size not equal:" + TC_Common::tostr(_pHead->_iMemSize) + "!=" + TC_Common::tostr(iSize));
        }

        if (_pHead->_iMinChunkSize != _iMinChunkSize)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::connect] hash map MinChunkSize not equal:" + TC_Common::tostr(_pHead->_iMinChunkSize) + "!=" + TC_Common::tostr(_iMinChunkSize) + " (data != code)");
        }

        if (_pHead->_iKeyType != _iKeyType)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::connect] keyType not equal:" + TC_Common::tostr(int(_pHead->_iKeyType)) + "!=" + TC_Common::tostr(int(_iKeyType)));
        }

        _pstCurrModify = _pstOuterModify;

        _iMainKeySize = _pHead->_iMainKeySize;
        _iDataSize = _pHead->_iDataSize;
        _fHashRatio = _pHead->_fHashRatio;
        _fMainKeyRatio = _pHead->_fMainKeyRatio;

        if (_iMainKeySize > 0 && _pMainKeyAllocator == NULL)
        {
            _pMainKeyAllocator = new BlockAllocator(this);
        }
        else
        {
            _pMainKeyAllocator = _pDataAllocator;
        }

        // 连接主key hash区
        void *pHashAddr = (char*)_pHead + sizeof(tagMapHead) + sizeof(tagModifyHead) * 2;
        _hashMainKey.connect(pHashAddr);

        // 连接联合hash区
        pHashAddr = (char*)pHashAddr + _hashMainKey.getMemSize();
        _hash.connect(pHashAddr);

        // 主key chunk区
        void *pDataAddr = (char*)pHashAddr + _hash.getMemSize();
        if (_iMainKeySize > 0)
        {
            _pMainKeyAllocator->connect(pDataAddr);
            pDataAddr = (char*)pDataAddr + _pMainKeyAllocator->getMemSize();
        }

        // 连接chunk区
        _pDataAllocator->connect(pDataAddr);


        // 恢复可能的错误
        doRecover();

        _tmpBufSize = 1024 * 1024 * 2;
        _tmpBuf = new char[_tmpBufSize];

        _iReadP = _pHead->_iGetHead;
        _iReadPBak = _pHead->_iGetHead;
    }

    int TC_Multi_HashMap_Malloc::append(void *pAddr, size_t iSize)
    {
        if (iSize <= _pHead->_iMemSize)
        {
            return -1;
        }

        init(pAddr);

        if (!_pHead->_bInit)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::append] hash map created unsuccessfully, so can not be appended");
        }

        if (_pHead->_cMaxVersion != MAX_VERSION || _pHead->_cMinVersion != MIN_VERSION)
        {
            // 版本不匹配
            ostringstream os;
            os << (int)_pHead->_cMaxVersion << "." << (int)_pHead->_cMinVersion << " != " << ((int)MAX_VERSION) << "." << ((int)MIN_VERSION);
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::append] hash map version not equal:" + os.str() + " (data != code)");
        }

        if (_pHead->_iMinChunkSize != _iMinChunkSize)
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::append] hash map MinChunkSize not equal:" + TC_Common::tostr(_pHead->_iMinChunkSize) + "!=" + TC_Common::tostr(_iMinChunkSize) + " (data != code)");
        }

        _pstCurrModify = _pstOuterModify;

        _iMainKeySize = _pHead->_iMainKeySize;
        _iDataSize = _pHead->_iDataSize;
        _fHashRatio = _pHead->_fHashRatio;
        _fMainKeyRatio = _pHead->_fMainKeyRatio;

        if (_iMainKeySize > 0 && _pMainKeyAllocator == NULL)
        {
            _pMainKeyAllocator = new BlockAllocator(this);
        }
        else
        {
            _pMainKeyAllocator = _pDataAllocator;
        }

        // 连接主key hash区
        void *pHashAddr = (char*)_pHead + sizeof(tagMapHead) + sizeof(tagModifyHead) * 2;
        _hashMainKey.connect(pHashAddr);

        // 连接联合hash区
        pHashAddr = (char*)pHashAddr + _hashMainKey.getMemSize();
        _hash.connect(pHashAddr);

        // 主key chunk区
        // 注意在主key为单独的chunk分配器的情况下，理论上不应该使用内存扩展功能
        void *pDataAddr = (char*)pHashAddr + _hash.getMemSize();
        if (_iMainKeySize > 0)
        {
            _pMainKeyAllocator->connect(pDataAddr);
            pDataAddr = (char*)pDataAddr + _pMainKeyAllocator->getMemSize();
        }

        // 连接chunk区
        _pDataAllocator->connect(pDataAddr);
        if ((uint64_t)(_pDataAllocator->getAllCapacity() + (iSize - _pHead->_iMemSize)) > ((uint64_t)((uint32_t)(-1) >> 9)*(1 << 9)*_iMinChunkSize))
        {
            throw TC_Multi_HashMap_Malloc_Exception("[TC_Multi_HashMap_Malloc::append] append mem size too large");
        }
        // 扩展chunk区
        _pDataAllocator->append(pDataAddr, iSize - ((size_t)pDataAddr - (size_t)pAddr));

        _pHead->_iMemSize = iSize;

        return 0;
    }

    void TC_Multi_HashMap_Malloc::clear()
    {
        assert(_pHead);

        _pHead->_bInit = false;
        _pHead->_iElementCount = 0;
        _pHead->_iMainKeyCount = 0;
        _pHead->_iSetHead = 0;
        _pHead->_iSetTail = 0;
        _pHead->_iGetHead = 0;
        _pHead->_iGetTail = 0;
        _pHead->_iDirtyCount = 0;
        _pHead->_iDirtyTail = 0;
        _pHead->_iDataUsedChunk = 0;
        _pHead->_iUsedDataMem = 0;
        _pHead->_iMKUsedChunk = 0;
        _pHead->_iMKUsedMem = 0;
        _pHead->_iGetCount = 0;
        _pHead->_iHitCount = 0;
        _pHead->_iBackupTail = 0;
        _pHead->_iSyncTail = 0;

        _hashMainKey.clear();
        _hash.clear();

        // 清除错误
        doUpdate();

        if (_iMainKeySize > 0)
        {
            _pMainKeyAllocator->rebuild();
        }

        _pDataAllocator->rebuild();
        _pHead->_bInit = true;
    }

    int TC_Multi_HashMap_Malloc::dump2file(const string &sFile)
    {
        FILE *fp = fopen(sFile.c_str(), "wb");
        if (fp == NULL)
        {
            return RT_DUMP_FILE_ERR;
        }

        size_t ret = fwrite((void*)_pHead, 1, _pHead->_iMemSize, fp);
        fclose(fp);

        if (ret == _pHead->_iMemSize)
        {
            return RT_OK;
        }
        return RT_DUMP_FILE_ERR;
    }

    int TC_Multi_HashMap_Malloc::load5file(const string &sFile)
    {
        FILE *fp = fopen(sFile.c_str(), "rb");
        if (fp == NULL)
        {
            return RT_LOAD_FILE_ERR;
        }
        fseek(fp, 0L, SEEK_END);
        size_t fs = ftell(fp);
        if (fs != _pHead->_iMemSize)
        {
            fclose(fp);
            return RT_LOAD_FILE_ERR;
        }

        fseek(fp, 0L, SEEK_SET);

        size_t iSize = 1024 * 1024 * 10;
        size_t iLen = 0;
        char *pBuffer = new char[iSize];
        bool bEof = false;
        while (true)
        {
            int ret = fread(pBuffer, 1, iSize, fp);
            if (ret != (int)iSize)
            {
                if (feof(fp))
                {
                    bEof = true;
                }
                else
                {
                    delete[] pBuffer;
                    return RT_LOAD_FILE_ERR;
                }

            }
            //检查版本
            if (iLen == 0)
            {
                if (pBuffer[0] != MAX_VERSION || pBuffer[1] != MIN_VERSION)
                {
                    delete[] pBuffer;
                    return RT_VERSION_MISMATCH_ERR;
                }
            }

            memcpy((char*)_pHead + iLen, pBuffer, ret);
            iLen += ret;
            if (bEof)
                break;
        }
        fclose(fp);
        delete[] pBuffer;
        if (iLen == _pHead->_iMemSize)
        {
            return RT_OK;
        }

        return RT_LOAD_FILE_ERR;
    }

    int TC_Multi_HashMap_Malloc::checkMainKey(const string &mk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        int ret = RT_OK;
        uint32_t index = mhashIndex(mk);
        if (itemMainKey(index)->_iMainKeyAddr == 0)
        {
            return RT_NO_DATA;
        }

        MainKey mainKey(this, itemMainKey(index)->_iMainKeyAddr);
        do
        {
            string s;
            if (_pHead->_iKeyType != MainKey::ZSET_TYPE)
                ret = mainKey.get(s);
            else
                ret = mainKey.getData(s);

            if (ret != RT_OK)
            {
                return ret;
            }
            if (s == mk)
            {
                incHitCount();
                // 找到了
                if (_pHead->_iKeyType != MainKey::ZSET_TYPE && mainKey.getHeadPtr()->_iBlockHead == 0)
                {
                    return RT_ONLY_KEY;
                }
                else if (_pHead->_iKeyType == MainKey::ZSET_TYPE && mainKey.getNext(0) == 0)
                {
                    return RT_ONLY_KEY;
                }
                else
                {
                    //如果有数据，但_iBlockCount为0，表示主key下的数据都删除了，返回RT_DATA_DEL
                    if ((mainKey.getHeadPtr()->_iBlockCount == 0) && (mainKey.ISFULLDATA()))
                        return RT_DATA_DEL;

                    if (!mainKey.ISFULLDATA())
                    {
                        return RT_PART_DATA;
                    }
                }
                return RT_OK;
            }

        } while (mainKey.next());

        return RT_NO_DATA;
    }

    int TC_Multi_HashMap_Malloc::getMainKeyType(TC_Multi_HashMap_Malloc::MainKey::KEYTYPE &keyType)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        keyType = (TC_Multi_HashMap_Malloc::MainKey::KEYTYPE)(_pHead->_iKeyType);

        /*incGetCount();

        int ret          = RT_OK;
        uint32_t index   = mhashIndex(mk);
        if(itemMainKey(index)->_iMainKeyAddr == 0)
        {
            return RT_NO_DATA;
        }

        MainKey mainKey(this, itemMainKey(index)->_iMainKeyAddr);
        do
        {
            string s;
            if(_pHead->_iKeyType != MainKey::ZSET_TYPE)
                ret = mainKey.get(s);
            else
                ret = mainKey.getData(s);

            if(ret != RT_OK)
            {
                return ret;
            }
            if(s == mk)
            {
                keyType = (TC_Multi_HashMap_Malloc::MainKey::KEYTYPE)(_pHead->_iKeyType);
                return RT_OK;
            }

        } while (mainKey.next());

        return RT_NO_DATA;*/

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::setFullData(const string &mk, bool bFull)
    {
        if (_pHead->_bReadOnly) return RT_READONLY;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);

        incGetCount();

        int ret = RT_OK;
        uint32_t index = mhashIndex(mk);
        if (itemMainKey(index)->_iMainKeyAddr == 0)
        {
            return RT_NO_DATA;
        }

        MainKey mainKey(this, itemMainKey(index)->_iMainKeyAddr);
        do
        {
            string s;
            if (MainKey::ZSET_TYPE != keyType)
                ret = mainKey.get(s);
            else
                ret = mainKey.getData(s);
            if (ret != RT_OK)
            {
                return ret;
            }
            if (s == mk)
            {
                incHitCount();
                // 找到了
                mainKey.SETFULLDATA(bFull);
                return RT_OK;
            }

        } while (mainKey.next());

        return RT_NO_DATA;
    }

    int TC_Multi_HashMap_Malloc::checkDirty(const string &mk, const string &uk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::LIST_TYPE == keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it;
        if (MainKey::HASH_TYPE == keyType)
            it = find(mk, uk, index, ret);
        else if (MainKey::SET_TYPE == keyType)
            it = findSet(mk, uk, index, ret);
        else if (MainKey::ZSET_TYPE == keyType)
            it = findZSet(mk, uk, index, ret);

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        //没有数据
        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        incHitCount();

        //只有Key
        if (it->isOnlyKey())
        {
            return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
        }

        Block block(this, it->getAddr());
        if (block.isDelete())
        {
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;
        }
        if (block.isDirty())
        {
            return TC_Multi_HashMap_Malloc::RT_DIRTY_DATA;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::checkDirty(const string &mk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        int ret = RT_OK;
        uint32_t index = mhashIndex(mk);
        if (itemMainKey(index)->_iMainKeyAddr == 0)
        {
            return RT_NO_DATA;
        }

        MainKey mainKey(this, itemMainKey(index)->_iMainKeyAddr);
        do
        {
            string s;
            ret = mainKey.get(s);
            if (ret != RT_OK)
            {
                return ret;
            }
            if (s == mk)
            {
                incHitCount();
                // 找到了
                if (mainKey.getHeadPtr()->_iBlockHead == 0)
                {
                    ret = RT_ONLY_KEY;
                }
                lock_iterator it(this, mainKey.getHeadPtr()->_iBlockHead, lock_iterator::IT_MKEY, lock_iterator::IT_NEXT);
                while (it != end())
                {
                    if ((it->isDelete() == false) && (it->isDirty()))
                    {
                        if (_pHead->_iDirtyTail == 0)
                        {
                            _pHead->_iDirtyTail = mainKey.getHeadPtr()->_iBlockHead;
                        }
                        return RT_DIRTY_DATA;
                    }
                    it++;
                }
                return RT_OK;
            }

        } while (mainKey.next());

        return RT_NO_DATA;
    }

    int TC_Multi_HashMap_Malloc::setDirty(const string &mk, const string &uk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        if (_pHead->_bReadOnly) return RT_READONLY;

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        //没有数据或只有Key
        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        incHitCount();

        //只有Key
        if (it->isOnlyKey())
        {
            return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
        }

        Block block(this, it->getAddr());
        if (block.isDelete())
        {
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;
        }
        block.setDirty(true);
        block.refreshSetList();

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::setDirtyAfterSync(const string &mk, const string &uk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        if (_pHead->_bReadOnly) return RT_READONLY;

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        //没有数据或只有Key
        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        incHitCount();

        //只有Key
        if (it->isOnlyKey())
        {
            return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
        }

        Block block(this, it->getAddr());
        if (block.isDelete())
        {
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;
        }
        block.setDirty(true);
        if (_pHead->_iDirtyTail == block.getBlockHead()->_iSetPrev)
        {
            _pHead->_iDirtyTail = block.getHead();
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::setClean(const string &mk, const string &uk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        if (_pHead->_bReadOnly) return RT_READONLY;

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        //没有数据或只有Key
        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        incHitCount();

        //只有Key
        if (it->isOnlyKey())
        {
            return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
        }

        Block block(this, it->getAddr());
        if (block.isDelete())
        {
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;
        }
        block.setDirty(false);
        block.refreshSetList();

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::setSyncTime(const string &mk, const string &uk, uint32_t iSyncTime)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        if (_pHead->_bReadOnly) return RT_READONLY;

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        //没有数据或只有Key
        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        incHitCount();

        //只有Key
        if (it->isOnlyKey())
        {
            return TC_Multi_HashMap_Malloc::RT_ONLY_KEY;
        }

        Block block(this, it->getAddr());
        if (block.isDelete())
        {
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;
        }
        block.setSyncTime(iSyncTime);

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    void TC_Multi_HashMap_Malloc::incMainKeyBlockCount(const string &mk, bool bInc)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);

        uint32_t index = mhashIndex(mk);

        // 找到真正的主key头
        if (itemMainKey(index)->_iMainKeyAddr != 0)
        {
            MainKey mainKey(this, itemMainKey(index)->_iMainKeyAddr);
            do
            {
                string sKey;
                if (MainKey::ZSET_TYPE != keyType)
                    mainKey.get(sKey);
                else
                    mainKey.getData(sKey);
                if (mk == sKey)
                {
                    // 找到了，增加计数
                    if (bInc)
                    {
                        saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);
                        updateMaxMainKeyBlockCount(mainKey.getHeadPtr()->_iBlockCount + 1);
                    }
                    else
                    {
                        saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount - 1);
                        updateMaxMainKeyBlockCount(mainKey.getHeadPtr()->_iBlockCount - 1);
                    }
                    break;
                }
            } while (mainKey.next());
        }
    }

    void TC_Multi_HashMap_Malloc::updateMaxMainKeyBlockCount(size_t iCount)
    {
        if (iCount > _pHead->_iMaxBlockCount)
        {
            saveValue(&_pHead->_iMaxBlockCount, iCount);
        }
    }


    int TC_Multi_HashMap_Malloc::insertSkipListSpan(MainKey &mainKey, uint32_t blockAddr)
    {
        //_pstCurrModify = _pstInnerModify;

        Block delBlock(this, blockAddr);

        double score = delBlock.getScore();

        int level = mainKey.getLevel();

        int currLevel = level - 1;

        Node *curr = &mainKey;

        Block currBlock(this, blockAddr);

        while (currLevel >= 0)
        {
            uint32_t addr = curr->getNext(currLevel);
            if (addr == 0)
            {
                currLevel--;
                continue;
            }
            else
            {
                Block block(this, addr);
                if (block.getScore() < score)
                {
                    currBlock = block;
                    curr = &currBlock;
                    continue;
                }
                else if (block.getScore() > score)
                {
                    curr->setSpan(currLevel, curr->getSpan(currLevel) + 1);
                    currLevel--;
                    continue;
                }
                else
                {
                    if (addr == blockAddr)
                    {
                        curr->setSpan(currLevel, curr->getSpan(currLevel) + 1);
                        /*saveValue(curr->Next(currLevel), *(delBlock.Next(currLevel)));
                        if(0 == currLevel && 0 != *(delBlock.Next(currLevel)))
                        {
                            Block NextBlock(this, *(delBlock.Next(currLevel)));
                            saveValue((uint32_t*)&(NextBlock.getBlockHead()->_iMKBlockPrev), currBlock.getHead());
                        }

                        if ((currLevel == 0) && (mainKey.getHeadPtr()->_iBlockTail == blockAddr))
                        {
                            assert(0 == *(delBlock.Next(currLevel)));
                            saveValue((uint32_t*)&(mainKey.getHeadPtr()->_iBlockTail), currBlock.getHead());
                        }*/
                        currLevel--;
                        continue;
                    }
                    else
                    {
                        if (currLevel > (delBlock.getLevel() - 1))
                        {//这个情况下要找删除的节点是在addr的前面还是后面
                            bool bFound = false;
                            Block tmpCurrBlock(this, addr);

                            while (tmpCurrBlock.getNext(0) != 0)
                            {
                                Block nextBlock(this, tmpCurrBlock.getNext(0));

                                if (nextBlock.getScore() == score && nextBlock.getHead() != blockAddr)
                                {
                                    tmpCurrBlock = nextBlock;
                                }
                                else if (nextBlock.getHead() == blockAddr)
                                {
                                    bFound = true;
                                    break;
                                }
                                else if (nextBlock.getScore() != score)
                                {
                                    break;
                                }
                                else
                                    break;
                            }

                            if (bFound)
                            {//表示在addr的后面
                                currBlock = block;
                                curr = &currBlock;
                                continue;
                            }
                            else
                            {//如果在前面，就要，span值加一
                                curr->setSpan(currLevel, curr->getSpan(currLevel) + 1);
                            }

                            currLevel--;
                            continue;
                        }

                        bool bFound = false;
                        Block tmpCurrBlock(this, addr);

                        while (tmpCurrBlock.getNext(currLevel) != 0)
                        {
                            Block nextBlock(this, tmpCurrBlock.getNext(currLevel));

                            if (nextBlock.getScore() == score && nextBlock.getHead() != blockAddr)
                            {
                                tmpCurrBlock = nextBlock;
                            }
                            else if (nextBlock.getHead() == blockAddr)
                            {
                                bFound = true;
                                break;
                            }
                            else if (nextBlock.getScore() != score)
                            {
                                break;
                            }
                            else
                                break;
                        }

                        if (bFound)
                        {
                            currBlock = tmpCurrBlock;
                            curr = &currBlock;
                            continue;
                        }
                        else
                        {
                            assert(false);
                            currLevel--;
                            continue;
                        }
                    }
                }
            }

        }

        //doUpdate();

        //_pstCurrModify = _pstOuterModify;

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::delSkipListSpan(MainKey &mainKey, uint32_t blockAddr)
    {
        //_pstCurrModify = _pstInnerModify;

        Block delBlock(this, blockAddr);

        double score = delBlock.getScore();

        int level = mainKey.getLevel();

        int currLevel = level - 1;

        Node *curr = &mainKey;

        Block currBlock(this, blockAddr);

        while (currLevel >= 0)
        {
            uint32_t addr = curr->getNext(currLevel);
            if (addr == 0)
            {
                currLevel--;
                continue;
            }
            else
            {
                Block block(this, addr);
                if (block.getScore() < score)
                {
                    currBlock = block;
                    curr = &currBlock;
                    continue;
                }
                else if (block.getScore() > score)
                {
                    curr->setSpan(currLevel, curr->getSpan(currLevel) - 1);
                    currLevel--;
                    continue;
                }
                else
                {
                    if (addr == blockAddr)
                    {
                        curr->setSpan(currLevel, curr->getSpan(currLevel) - 1);
                        /*saveValue(curr->Next(currLevel), *(delBlock.Next(currLevel)));
                        if(0 == currLevel && 0 != *(delBlock.Next(currLevel)))
                        {
                            Block NextBlock(this, *(delBlock.Next(currLevel)));
                            saveValue((uint32_t*)&(NextBlock.getBlockHead()->_iMKBlockPrev), currBlock.getHead());
                        }

                        if ((currLevel == 0) && (mainKey.getHeadPtr()->_iBlockTail == blockAddr))
                        {
                            assert(0 == *(delBlock.Next(currLevel)));
                            saveValue((uint32_t*)&(mainKey.getHeadPtr()->_iBlockTail), currBlock.getHead());
                        }*/
                        currLevel--;
                        continue;
                    }
                    else
                    {
                        if (currLevel > (delBlock.getLevel() - 1))
                        {//这个情况下要找删除的节点是在addr的前面还是后面
                            bool bFound = false;
                            Block tmpCurrBlock(this, addr);

                            while (tmpCurrBlock.getNext(0) != 0)
                            {
                                Block nextBlock(this, tmpCurrBlock.getNext(0));

                                if (nextBlock.getScore() == score && nextBlock.getHead() != blockAddr)
                                {
                                    tmpCurrBlock = nextBlock;
                                }
                                else if (nextBlock.getHead() == blockAddr)
                                {
                                    bFound = true;
                                    break;
                                }
                                else if (nextBlock.getScore() != score)
                                {
                                    break;
                                }
                                else
                                    break;
                            }

                            if (bFound)
                            {//表示在addr的后面
                                currBlock = block;
                                curr = &currBlock;
                                continue;
                            }
                            else
                            {//如果在前面，就要，span值减一
                                curr->setSpan(currLevel, curr->getSpan(currLevel) - 1);
                            }

                            currLevel--;
                            continue;
                        }

                        bool bFound = false;
                        Block tmpCurrBlock(this, addr);

                        while (tmpCurrBlock.getNext(currLevel) != 0)
                        {
                            Block nextBlock(this, tmpCurrBlock.getNext(currLevel));

                            if (nextBlock.getScore() == score && nextBlock.getHead() != blockAddr)
                            {
                                tmpCurrBlock = nextBlock;
                            }
                            else if (nextBlock.getHead() == blockAddr)
                            {
                                bFound = true;
                                break;
                            }
                            else if (nextBlock.getScore() != score)
                            {
                                break;
                            }
                            else
                                break;
                        }

                        if (bFound)
                        {
                            currBlock = tmpCurrBlock;
                            curr = &currBlock;
                            continue;
                        }
                        else
                        {
                            assert(false);
                            currLevel--;
                            continue;
                        }
                    }
                }
            }

        }

        //doUpdate();

        //_pstCurrModify = _pstOuterModify;

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::RandomHeight()
    {
        // Increase height with probability 1 in kBranching
        int height = 1;
        while (height < _pHead->_iMaxLevel && (_rand.Next() % 4) == 0)
            height++;

        assert(height > 0);
        assert(height <= _pHead->_iMaxLevel);

        return height;

        /*int height = 0;
        while(0 == height)
            height = _rand.Next() % _pHead->_iMaxLevel;

        return height;*/
    }

    int TC_Multi_HashMap_Malloc::insertSkipList(MainKey &mainKey, uint32_t blockAddr, double score)
    {
        //_pstCurrModify = _pstInnerModify;

        Block insertBlock(this, blockAddr);

        int blockLevel = insertBlock.getLevel();
        int level = mainKey.getLevel();

        assert(level >= blockLevel);

        vector<pair<bool, uint32_t> > vtSpanP;//记录需要修改span的节点信息, pair<是否为block，节点地址>
        vtSpanP.resize(level);
        vector<uint32_t> vtSpan;//记录节点的span值
        vtSpan.resize(level);

        int currLevel = level - 1;

        Node *curr = &mainKey;

        Block currBlock(this, blockAddr);

        while (currLevel >= 0)
        {
            assert(curr->getLevel() > currLevel);
            uint32_t addr = curr->getNext(currLevel);
            if (addr == 0)
            {
                if (currLevel > (blockLevel - 1))
                {
                    currLevel--;
                    continue;
                }

                insertBlock.setSpan(currLevel, 0);
                insertBlock.setNext(currLevel, 0);
                curr->setNext(currLevel, blockAddr);

                if ((void*)curr != (void*)&mainKey)
                {
                    vtSpanP[currLevel].first = true;
                    vtSpanP[currLevel].second = currBlock.getHead();
                }
                else
                {
                    vtSpanP[currLevel].first = false;
                    vtSpanP[currLevel].second = mainKey.getHead();
                }
                vtSpan[currLevel] = 0;

                if (0 == currLevel)
                {
                    if ((void*)curr == (void*)&mainKey)
                    {
                        saveValue((uint32_t*)&(insertBlock.getBlockHead()->_iMKBlockPrev), 0);
                    }
                    else
                    {
                        saveValue((uint32_t*)&(insertBlock.getBlockHead()->_iMKBlockPrev), currBlock.getHead());
                    }

                    saveValue((uint32_t*)&(mainKey.getHeadPtr()->_iBlockTail), blockAddr);

                    for (int i = currLevel + 1; i <= (blockLevel - 1); i++)
                        vtSpan[i] += 1;

                    curr->setSpan(0, 1);
                    vtSpan[currLevel] = 1;
                }
                currLevel--;
                continue;
            }
            else
            {
                Block block(this, addr);
                if (block.getScore() < score)
                {
                    if (currLevel <= (blockLevel - 1))
                    {
                        for (int i = currLevel + 1; i <= (blockLevel - 1); i++)
                            vtSpan[i] += curr->getSpan(currLevel);
                    }

                    currBlock = block;
                    curr = &currBlock;
                    continue;
                }
                else
                {
                    if (currLevel > (blockLevel - 1))
                    {
                        curr->setSpan(currLevel, curr->getSpan(currLevel) + 1);
                        currLevel--;
                        continue;
                    }

                    insertBlock.setSpan(currLevel, curr->getSpan(currLevel) + 1);
                    insertBlock.setNext(currLevel, addr);
                    curr->setNext(currLevel, blockAddr);

                    if ((void*)curr != (void*)&mainKey)
                    {
                        vtSpanP[currLevel].first = true;
                        vtSpanP[currLevel].second = currBlock.getHead();
                    }
                    else
                    {
                        vtSpanP[currLevel].first = false;
                        vtSpanP[currLevel].second = mainKey.getHead();
                    }
                    vtSpan[currLevel] = 0;

                    if (0 == currLevel)
                    {
                        if ((void*)curr == (void*)&mainKey)
                        {
                            saveValue((uint32_t*)&(insertBlock.getBlockHead()->_iMKBlockPrev), 0);
                        }
                        else
                        {
                            saveValue((uint32_t*)&(insertBlock.getBlockHead()->_iMKBlockPrev), currBlock.getHead());
                        }

                        saveValue((uint32_t*)&(block.getBlockHead()->_iMKBlockPrev), blockAddr);

                        for (int i = currLevel + 1; i <= (blockLevel - 1); i++)
                            vtSpan[i] += 1;

                        curr->setSpan(0, 1);
                        vtSpan[currLevel] = 1;
                    }

                    currLevel--;
                }
            }

        }
        //最后把span值写回去
        for (int i = 0; i < blockLevel; i++)
        {
            if (true == vtSpanP[i].first)
            {
                Block block(this, vtSpanP[i].second);
                block.setSpan(i, vtSpan[i]);
            }
            else
            {
                mainKey.setSpan(i, vtSpan[i]);
            }

            if (insertBlock.getNext(i) == 0)
                insertBlock.setSpan(i, 0);
            else
            {
                //assert(*(insertBlock.getSpan(i)) > vtSpan[i]);
                insertBlock.setSpan(i, insertBlock.getSpan(i) - vtSpan[i]);
            }
        }

        //doUpdate();

        //_pstCurrModify = _pstOuterModify;

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::delSkipList(MainKey &mainKey, uint32_t blockAddr)
    {
        //_pstCurrModify = _pstInnerModify;

        Block delBlock(this, blockAddr);
        bool isDeleted = delBlock.isDelete();
        double score = delBlock.getScore();

        int level = mainKey.getLevel();

        int currLevel = level - 1;

        Node *curr = &mainKey;

        Block currBlock(this, blockAddr);

        while (currLevel >= 0)
        {
            assert(curr->getLevel() > currLevel);
            uint32_t addr = curr->getNext(currLevel);
            if (addr == 0)
            {
                currLevel--;
                continue;
            }
            else
            {
                Block block(this, addr);
                if (block.getScore() < score)
                {
                    currBlock = block;
                    curr = &currBlock;
                    continue;
                }
                else if (block.getScore() > score)
                {
                    if (!isDeleted)
                        curr->setSpan(currLevel, curr->getSpan(currLevel) - 1);

                    currLevel--;
                    continue;
                }
                else
                {
                    if (addr == blockAddr)
                    {
                        if (!isDeleted)
                            curr->setSpan(currLevel, curr->getSpan(currLevel) + delBlock.getSpan(currLevel) - 1);
                        else
                            curr->setSpan(currLevel, curr->getSpan(currLevel) + delBlock.getSpan(currLevel));

                        curr->setNext(currLevel, delBlock.getNext(currLevel));
                        if (0 == currLevel && 0 != delBlock.getNext(currLevel))
                        {
                            Block NextBlock(this, delBlock.getNext(currLevel));
                            saveValue((uint32_t*)&(NextBlock.getBlockHead()->_iMKBlockPrev), delBlock.getBlockHead()->_iMKBlockPrev);
                        }

                        if ((currLevel == 0) && (mainKey.getHeadPtr()->_iBlockTail == blockAddr))
                        {
                            assert(0 == delBlock.getNext(currLevel));
                            saveValue((uint32_t*)&(mainKey.getHeadPtr()->_iBlockTail), delBlock.getBlockHead()->_iMKBlockPrev);
                        }
                        currLevel--;
                        continue;
                    }
                    else
                    {
                        if (currLevel > (delBlock.getLevel() - 1))
                        {//这个情况下要找删除的节点是在addr的前面还是后面
                            bool bFound = false;
                            Block tmpCurrBlock(this, addr);

                            while (tmpCurrBlock.getNext(0) != 0)
                            {
                                Block nextBlock(this, tmpCurrBlock.getNext(0));

                                if (nextBlock.getScore() == score && nextBlock.getHead() != blockAddr)
                                {
                                    tmpCurrBlock = nextBlock;
                                }
                                else if (nextBlock.getHead() == blockAddr)
                                {
                                    bFound = true;
                                    break;
                                }
                                else if (nextBlock.getScore() != score)
                                {
                                    break;
                                }
                                else
                                    break;
                            }

                            if (bFound)
                            {//表示在addr的后面
                                currBlock = block;
                                curr = &currBlock;
                                continue;
                            }
                            else
                            {//如果在前面，就要，span值减一
                                if (!isDeleted)
                                {
                                    curr->setSpan(currLevel, curr->getSpan(currLevel) - 1);
                                }
                            }

                            currLevel--;
                            continue;
                        }

                        bool bFound = false;
                        Block tmpCurrBlock(this, addr);

                        while (tmpCurrBlock.getNext(currLevel) != 0)
                        {
                            Block nextBlock(this, tmpCurrBlock.getNext(currLevel));

                            if (nextBlock.getScore() == score && nextBlock.getHead() != blockAddr)
                            {
                                tmpCurrBlock = nextBlock;
                            }
                            else if (nextBlock.getHead() == blockAddr)
                            {
                                bFound = true;
                                break;
                            }
                            else if (nextBlock.getScore() != score)
                            {
                                break;
                            }
                            else
                                break;
                        }

                        if (bFound)
                        {
                            currBlock = tmpCurrBlock;
                            curr = &currBlock;
                            continue;
                        }
                        else
                        {
                            assert(false);
                            currLevel--;
                            continue;
                        }
                    }
                }
            }

        }

        //doUpdate();

        //_pstCurrModify = _pstOuterModify;

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::get(const string &mk, const string &uk, const uint32_t unHash, Value &v, bool bCheckExpire /*=false*/, uint32_t iExpireTime /*=-1*/)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        int ret = RT_OK;
        lock_iterator it = find(mk, uk, unHash%_hash.size(), v, ret);

        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            // 没有数据
            // 查询主key信息
            uint32_t mIndex = mhashIndex(mk);
            uint32_t iAddr = find(mk, mIndex, ret);
            if (ret != RT_OK)
            {
                // 这里有可能返回RT_ONLY_KEY
                return ret;
            }
            if (iAddr != 0)
            {
                MainKey mainKey(this, iAddr);
                if (mainKey.ISFULLDATA() || mainKey.getHeadPtr()->_iBlockHead == 0)
                {
                    // 主key存在，由于cache中的数据必须与数据源(如数据库)保持一致
                    // 说明数据源必定没有此记录，返回only key即可，可以使得调用者不必
                    // 从数据源再取数据
                    return RT_ONLY_KEY;
                }
                // 如果主key存在，但数据不全，则有可能会在数据库中有，应该返回RT_NO_DATA
            }
            return RT_NO_DATA;
        }

        Block block(this, it->getAddr());

        //已经标记为删除
        if (block.isDelete())
            return RT_DATA_DEL;

        incHitCount();

        // 如果需要检查数据是否已过期，数据过期时间不为0且已经小于iExpireTime的话，返回RT_DATE_EXPIRED
        if (bCheckExpire && v._iExpireTime != 0 && v._iExpireTime < iExpireTime)
        {
            return RT_DATA_EXPIRED;
        }

        // 只有Key
        if (block.isOnlyKey())
        {
            return RT_ONLY_KEY;
        }

        // 如果只读, 则不刷新get链表
        if (!_pHead->_bReadOnly)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.refreshGetList();
        }
        return RT_OK;
    }
    int TC_Multi_HashMap_Malloc::get(const string &mk, const string &uk, const uint32_t unHash, Value &v, uint32_t &iSyncTime, uint32_t& iDateExpireTime, uint8_t& iVersion, bool& bDirty, bool bCheckExpire, uint32_t iExpireTime)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        int ret = RT_OK;
        lock_iterator it = find(mk, uk, unHash%_hash.size(), v, ret);

        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            // 没有数据
            // 查询主key信息
            uint32_t mIndex = mhashIndex(mk);
            uint32_t iAddr = find(mk, mIndex, ret);
            if (ret != RT_OK)
            {
                // 这里有可能返回RT_ONLY_KEY
                return ret;
            }
            if (iAddr != 0)
            {
                MainKey mainKey(this, iAddr);
                if (mainKey.ISFULLDATA() || mainKey.getHeadPtr()->_iBlockHead == 0)
                {
                    // 主key存在，由于cache中的数据必须与数据源(如数据库)保持一致
                    // 说明数据源必定没有此记录，返回only key即可，可以使得调用者不必
                    // 从数据源再取数据
                    return RT_ONLY_KEY;
                }
                // 如果主key存在，但数据不全，则有可能会在数据库中有，应该返回RT_NO_DATA
            }
            return RT_NO_DATA;
        }

        Block block(this, it->getAddr());

        //已经标记为删除
        if (block.isDelete())
            return RT_DATA_DEL;

        incHitCount();

        iVersion = block.getVersion();
        iDateExpireTime = block.getExpireTime();
        bDirty = block.isDirty();
        iSyncTime = block.getSyncTime();

        // 如果需要检查数据是否已过期，数据过期时间不为0且已经小于iExpireTime的话，返回RT_DATE_EXPIRED
        if (bCheckExpire && v._iExpireTime != 0 && v._iExpireTime < iExpireTime)
        {
            return RT_DATA_EXPIRED;
        }

        // 只有Key
        if (block.isOnlyKey())
        {
            return RT_ONLY_KEY;
        }

        // 如果只读, 则不刷新get链表
        if (!_pHead->_bReadOnly)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.refreshGetList();
        }
        return RT_OK;
    }
    int TC_Multi_HashMap_Malloc::get(const string &mk, vector<Value> &vs, size_t iCount /*= -1*/,
        size_t iStart /*= 0*/, bool bHead /*= true*/,
        bool bCheckExpire /*=false*/, uint32_t iExpireTime /*=-1*/)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        incHitCount();

        MainKey mainKey(this, iMainKeyAddr);
        if (mainKey.getHeadPtr()->_iBlockHead == 0)
        {
            return RT_ONLY_KEY;
        }
        uint32_t iAddr = 0;
        if (bHead)
        {
            iAddr = mainKey.getHeadPtr()->_iBlockHead;
        }
        else
        {
            iAddr = mainKey.getHeadPtr()->_iBlockTail;
        }

        uint32_t iBlockCount = mainKey.getHeadPtr()->_iBlockCount;
        if (iCount > iBlockCount)
            vs.reserve(iBlockCount);
        else
            vs.reserve(iCount);

        size_t iIndex = 0, iGetCount = 0;
        while (iAddr != 0 && iGetCount < iCount)
        {
            Block block(this, iAddr);
            if ((block.isDelete() == false) && (iIndex++ >= iStart))
            {
                BlockData data;
                ret = block.getBlockData(data);
                if (ret != RT_OK && ret != RT_ONLY_KEY)
                {
                    return ret;
                }
                if (ret == RT_OK)
                {
                    // 仅取非only key的数据
                    if (!bCheckExpire || data._expire == 0 || data._expire > iExpireTime)
                    {
                        // 如果需要检查数据是否已过期，数据过期时间不为0且已经小于iExpireTime的话，则不取该数据
                        Value v(mk, data);
                        vs.push_back(v);
                        iGetCount++;
                    }
                }
            }
            if (bHead)
            {
                iAddr = block.getBlockHead()->_iMKBlockNext;
            }
            else
            {
                iAddr = block.getBlockHead()->_iMKBlockPrev;
            }
        }

        if (mainKey.ISFULLDATA() && (iGetCount == 0))
            return RT_DATA_DEL;

        //修复老版本 get完以后没有刷新get链
        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        if (!mainKey.ISFULLDATA())
        {
            return RT_PART_DATA;
        }

        return RT_OK;
    }
    int TC_Multi_HashMap_Malloc::get(const string &mk, size_t &iDataCount)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        incHitCount();

        MainKey mainKey(this, iMainKeyAddr);


        if (mainKey.getHeadPtr()->_iBlockHead == 0)
        {
            return RT_ONLY_KEY;
        }

        iDataCount = mainKey.getHeadPtr()->_iBlockCount;

        return RT_OK;
    }
    //为了支持
    int TC_Multi_HashMap_Malloc::get(const string &mk, const string &uk, const uint32_t unHash, vector<Value> &vs, size_t iCount /*= -1*/,
        bool bForward /*= true*/, bool bCheckExpire/*=false*/, uint32_t iExpireTime/*=-1*/)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        int ret = RT_OK;
        Value tmpValue;
        lock_iterator it = find(mk, uk, unHash%_hash.size(), tmpValue, ret);

        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            // 没有数据
            // 查询主key信息
            uint32_t mIndex = mhashIndex(mk);
            uint32_t iAddr = find(mk, mIndex, ret);
            if (ret != RT_OK)
            {
                // 这里有可能返回RT_ONLY_KEY
                return ret;
            }
            if (iAddr != 0)
            {
                MainKey mainKey(this, iAddr);
                if (mainKey.ISFULLDATA() || mainKey.getHeadPtr()->_iBlockHead == 0)
                {
                    // 主key存在，由于cache中的数据必须与数据源(如数据库)保持一致
                    // 说明数据源必定没有此记录，返回only key即可，可以使得调用者不必
                    // 从数据源再取数据
                    return RT_ONLY_KEY;
                }
                // 如果主key存在，但数据不全，则有可能会在数据库中有，应该返回RT_NO_DATA
            }
            return RT_NO_DATA;
        }

        incHitCount();

        size_t iGetCount = 0;
        uint32_t iAddr = it->getAddr();

        Block block(this, iAddr);
        MainKey mainKey(this, block.getBlockHead()->_iMainKey);

        if (bForward)
        {
            iAddr = block.getBlockHead()->_iMKBlockNext;
        }
        else
        {
            iAddr = block.getBlockHead()->_iMKBlockPrev;
        }

        uint32_t iBlockCount = mainKey.getHeadPtr()->_iBlockCount;
        if (iCount > iBlockCount)
            vs.reserve(iBlockCount);
        else
            vs.reserve(iCount);

        while (iAddr != 0 && iGetCount < iCount)
        {
            Block block(this, iAddr);
            if (block.isDelete() == false)
            {
                BlockData data;
                ret = block.getBlockData(data);
                if (ret != RT_OK && ret != RT_ONLY_KEY)
                {
                    return ret;
                }
                if (ret == RT_OK)
                {
                    // 仅取非only key的数据
                    if (!bCheckExpire || data._expire == 0 || data._expire > iExpireTime)
                    {
                        // 如果需要检查数据是否已过期，数据过期时间不为0且已经小于iExpireTime的话，则不取该数据
                        Value v(mk, data);
                        vs.push_back(v);
                        iGetCount++;
                    }
                }
            }
            if (bForward)
            {
                iAddr = block.getBlockHead()->_iMKBlockNext;
            }
            else
            {
                iAddr = block.getBlockHead()->_iMKBlockPrev;
            }
        }

        //修复老版本 get完以后没有刷新get链
        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        if (!mainKey.ISFULLDATA())
        {
            return RT_PART_DATA;
        }

        return RT_OK;
    }
    int TC_Multi_HashMap_Malloc::get(uint32_t &mh, map<string, vector<Value> > &vs)
    {
        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mh % _hashMainKey.size();
        uint32_t iMainKeyAddr = itemMainKey(index)->_iMainKeyAddr;
        if (iMainKeyAddr == 0)
        {
            return RT_OK;
        }

        MainKey mainKey(this, iMainKeyAddr);
        do
        {
            string mk;
            if (_pHead->_iKeyType != MainKey::ZSET_TYPE)
                ret = mainKey.get(mk);
            else
                ret = mainKey.getData(mk);

            if (mh == mhash(mk))
            {
                if (_pHead->_iKeyType == MainKey::HASH_TYPE && mainKey.getHeadPtr()->_iBlockHead != 0)
                {
                    uint32_t iBlockCount = mainKey.getHeadPtr()->_iBlockCount;
                    map<string, vector<Value> >::iterator mIt = vs.end();

                    lock_iterator it(this, mainKey.getHeadPtr()->_iBlockHead, lock_iterator::IT_MKEY, lock_iterator::IT_NEXT);
                    while (it != end())
                    {
                        Value v;
                        ret = it->get(v);
                        if (ret != RT_OK && ret != RT_ONLY_KEY)
                        {
                            return ret;
                        }
                        //if(ret == RT_OK)
                        {
                            if (mIt == vs.end())
                            {
                                vs[v._mkey].reserve(iBlockCount);
                                mIt = vs.find(v._mkey);
                            }

                            mIt->second.push_back(v);
                        }
                        it++;
                    }
                }
                else if (_pHead->_iKeyType == MainKey::LIST_TYPE && mainKey.getHeadPtr()->_iBlockHead != 0)
                {
                    uint32_t iBlockCount = mainKey.getHeadPtr()->_iBlockCount;
                    map<string, vector<Value> >::iterator mIt = vs.end();

                    lock_iterator it(this, mainKey.getHeadPtr()->_iBlockHead, lock_iterator::IT_MKEY, lock_iterator::IT_NEXT);
                    while (it != end())
                    {
                        Value v;
                        ret = it->getList(v);
                        if (ret != RT_OK && ret != RT_ONLY_KEY)
                        {
                            return ret;
                        }
                        //if(ret == RT_OK)
                        {
                            if (mIt == vs.end())
                            {
                                vs[v._mkey].reserve(iBlockCount);
                                mIt = vs.find(v._mkey);
                            }

                            mIt->second.push_back(v);
                        }
                        it++;
                    }
                }
                else if (_pHead->_iKeyType == MainKey::SET_TYPE && mainKey.getHeadPtr()->_iBlockHead != 0)
                {
                    uint32_t iBlockCount = mainKey.getHeadPtr()->_iBlockCount;
                    map<string, vector<Value> >::iterator mIt = vs.end();

                    lock_iterator it(this, mainKey.getHeadPtr()->_iBlockHead, lock_iterator::IT_MKEY, lock_iterator::IT_NEXT);
                    while (it != end())
                    {
                        Value v;
                        ret = it->getSet(v);
                        if (ret != RT_OK && ret != RT_ONLY_KEY)
                        {
                            return ret;
                        }
                        //if(ret == RT_OK)
                        {
                            if (mIt == vs.end())
                            {
                                vs[v._mkey].reserve(iBlockCount);
                                mIt = vs.find(v._mkey);
                            }

                            mIt->second.push_back(v);
                        }
                        it++;
                    }
                }
                else if (_pHead->_iKeyType == MainKey::ZSET_TYPE && mainKey.getNext(0) != 0)
                {
                    string sMainKey;
                    mainKey.getData(sMainKey);

                    uint32_t iAddr = mainKey.getNext(0);

                    vector<Value> vt;
                    vt.reserve(10000);

                    while (iAddr != 0)
                    {
                        Block block(this, iAddr);
                        iAddr = block.getNext(0);

                        Value data;
                        data._isDelete = block.isDelete() ? DELETE_TRUE : DELETE_FALSE;
                        data._dirty = block.isDirty();
                        data._iSyncTime = block.getSyncTime();
                        data._iVersion = block.getVersion();
                        data._iExpireTime = block.getExpireTime();
                        data._keyType = MainKey::ZSET_TYPE;
                        data._score = block.getScore();
                        block.getData(data._value);
                        vt.push_back(data);
                    }

                    vs[sMainKey].swap(vt);
                }
                else
                {
                    vector<Value> vecTmp;
                    string mkey;

                    if (_pHead->_iKeyType != MainKey::ZSET_TYPE)
                        ret = mainKey.get(mkey);
                    else
                        ret = mainKey.getData(mkey);

                    if (ret != RT_OK)
                    {
                        return ret;
                    }
                    vs[mkey] = vecTmp;
                }
            }
        } while (mainKey.next());

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::get(uint32_t &mh, map<string, vector<PackValue> > &vs)
    {
        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mh % _hashMainKey.size();
        uint32_t iMainKeyAddr = itemMainKey(index)->_iMainKeyAddr;
        if (iMainKeyAddr == 0)
        {
            return RT_OK;
        }

        MainKey mainKey(this, iMainKeyAddr);
        do
        {
            if (mainKey.getHeadPtr()->_iBlockHead != 0)
            {
                uint32_t iBlockCount = mainKey.getHeadPtr()->_iBlockCount;
                map<string, vector<PackValue> >::iterator mIt = vs.end();

                lock_iterator it(this, mainKey.getHeadPtr()->_iBlockHead, lock_iterator::IT_MKEY, lock_iterator::IT_NEXT);
                while (it != end())
                {
                    PackValue v;
                    ret = it->get(v);
                    if (ret != RT_OK && ret != RT_ONLY_KEY)
                    {
                        return ret;
                    }
                    if (ret == RT_OK)
                    {
                        // 仅取非only key的数据
                        if (mIt == vs.end())
                        {
                            vs[v._mkey].reserve(iBlockCount);
                            mIt = vs.find(v._mkey);
                        }

                        mIt->second.push_back(v);
                    }
                    ++it;
                }
            }
        } while (mainKey.next());

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::setForDel(const string &mk, const string &uk, const time_t t, DATATYPE eType, bool bHead, vector<Value> &vtData)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, ret);
        bool bNewBlock = false;

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        incHitCount();

        if (it != end())
        {
            // 如果已经存在，则设置删除标记位
            Block block(this, it->getAddr());

            //设置删除位
            block.setDelete(true);

            block.setSyncTime(t);

            block.setDirty(false);

            return RT_OK;
        }

        // 查找是否已经存在于主key链
        uint32_t mIndex = mhashIndex(mk);		// 主key索引
        uint32_t iMainKeyAddr = find(mk, mIndex, ret);
        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key头尚不存在，新建一个
            uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length());
            iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(mIndex, iAllocSize, vtData);
            if (iMainKeyAddr == 0)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }
            // 将主key设置到主key头中
            MainKey mainKey(this, iMainKeyAddr);
            ret = mainKey.set(mk.c_str(), mk.length(), vtData);
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                doRecover();
                return ret;
            }
        }

        TC_PackIn pi;
        pi << uk;
        uint32_t iAllocSize = (uint32_t)(sizeof(Block::tagBlockHead) + pi.length());

        //先分配空间, 并获得淘汰的数据
        uint32_t iAddr = _pDataAllocator->allocateMemBlock(MainKey::HASH_TYPE, iMainKeyAddr, index, bHead, iAllocSize, vtData);
        if (iAddr == 0)
        {
            doRecover();
            return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
        }

        it = HashMapLockIterator(this, iAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);
        bNewBlock = true;

        ret = it->set(mk, uk, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            // set数据失败，可能的情况是空间不够了
            if (bNewBlock)
            {
                // 如果是新set的数据，此时要注意删除前面分配并挂接的block头
                // 否则这个block将永远无法访问，也不能删除，因为block里是一个空的block
                //Block block(this, it->getAddr());
                //block.erase();
                doRecover();
            }
            return ret;
        }

        Block block(this, it->getAddr());
        block.setDelete(true);

        if (bNewBlock)
        {
            block.setDirty(false);
            block.setSyncTime(t);
        }

        if (eType != AUTO_DATA)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.SETFULLDATA(eType == FULL_DATA);
        }

        block.refreshSetList();

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::set(const string &mk, const string &uk, const uint32_t unHash, const string &v, uint32_t iExpireTime,
        uint8_t iVersion, bool bDirty, DATATYPE eType, bool bHead, bool bUpdateOrder, DELETETYPE deleteType, vector<Value> &vtData)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        lock_iterator it = find(mk, uk, unHash%_hash.size(), ret);
        bool bNewBlock = false;

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        incHitCount();

        uint32_t iOldAddr = 0;

        TC_PackIn pi;
        pi << uk;
        pi << v;
        uint32_t iAllocSize = (uint32_t)(sizeof(Block::tagBlockHead) + pi.length());

        if (it != end())
        {
            //如果数据存在，要判断是否需要重新分配内存
            iOldAddr = it->getAddr();
            Block block(this, iOldAddr);
            //数据已标记删除，直接return
            if (deleteType == DELETE_AUTO && block.isDelete())
            {
                return RT_DATA_DEL;
            }
            uint32_t iAddr = _pDataAllocator->relocateMemBlock(it->getAddr(), iVersion, block.getBlockHead()->_iMainKey, unHash%_hash.size(), bHead, iAllocSize, vtData);
            if (iAddr == 0)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }

            it = HashMapLockIterator(this, iAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);
        }
        else
        {
            // 数据尚不存在

            // 查找是否已经存在于主key链
            uint32_t mIndex = mhashIndex(mk);		// 主key索引
            uint32_t iMainKeyAddr = find(mk, mIndex, ret);
            if (ret != RT_OK && ret != RT_ONLY_KEY)
            {
                return ret;
            }
            if (iMainKeyAddr == 0)
            {
                // 主key头尚不存在，新建一个
                uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length());
                iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(mIndex, iAllocSize, vtData);
                if (iMainKeyAddr == 0)
                {
                    return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
                }
                // 将主key设置到主key头中
                MainKey mainKey(this, iMainKeyAddr);
                ret = mainKey.set(mk.c_str(), mk.length(), vtData);
                if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    doRecover();
                    return ret;
                }
            }

            // 先分配Blcok空间, 并获得淘汰的数据
            uint32_t iAddr = _pDataAllocator->allocateMemBlock(MainKey::HASH_TYPE, iMainKeyAddr, unHash%_hash.size(), bHead, iAllocSize, vtData);
            if (iAddr == 0)
            {
                doRecover();
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }

            it = HashMapLockIterator(this, iAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);
            iOldAddr = iAddr;
            bNewBlock = true;
        }

        bool bOldIsDelete = false;
        {
            Block block(this, iOldAddr);
            bOldIsDelete = block.isDelete();
        }

        ret = it->set(mk, uk, v, iExpireTime, iVersion, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            // set数据失败，可能的情况是空间不够了
            if (bNewBlock)
            {
                // 如果是新set的数据，此时要注意删除前面分配并挂接的block头
                // 否则这个block将永远无法访问，也不能删除，因为block里是一个空的block
                //Block block(this, it->getAddr());
                //block.erase();
                // modified by smitchzhao @ 2011-10-02
                doRecover();
            }
            else if (iOldAddr != it->getAddr())
            { //失败就要把新分配的块删除
                doRecover();
            }
            return ret;
        }
        else if (iOldAddr != it->getAddr())
        {//成功后，要把旧数据删除
            Block block(this, iOldAddr);
            if (block.isDelete())
            {
                // 把主key下数据个数增加一个，因为后面的block.erase()会把这个计数减一
                MainKey mainKey(this, block.getBlockHead()->_iMainKey);
                saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);
            }
            block.erase();
        }

        Block block(this, it->getAddr());
        if (bNewBlock)
        {
            block.setSyncTime(time(NULL));
        }
        block.setDirty(bDirty);

        if (deleteType != DELETE_AUTO)
        {
            block.setDelete(deleteType == DELETE_TRUE);
        }

        if (eType != AUTO_DATA)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.SETFULLDATA(eType == FULL_DATA);
        }

        block.refreshSetList();

        //如果不是新插入的，并且设置了更新重新排序
        // 或者之前是被删除的
        //就要重新更新key链下的顺序
        if ((!bNewBlock && bUpdateOrder) || (bOldIsDelete))
        {
            block.refreshKeyList(bHead);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::set(const string &mk, const string &uk, const uint32_t unHash, const string &v, uint32_t iExpireTime,
        uint8_t iVersion, bool bDirty, DATATYPE eType, bool bHead, bool bUpdateOrder, DELETETYPE deleteType,
        bool bCheckExpire, uint32_t iNowTime, vector<Value> &vtData)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        lock_iterator it = find(mk, uk, unHash%_hash.size(), ret);
        bool bNewBlock = false;

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        incHitCount();

        uint32_t iOldAddr = 0;

        TC_PackIn pi;
        pi << uk;
        pi << v;
        uint32_t iAllocSize = (uint32_t)(sizeof(Block::tagBlockHead) + pi.length());

        if (it != end())
        {
            //如果数据存在，要判断是否需要重新分配内存
            iOldAddr = it->getAddr();
            Block block(this, iOldAddr);
            //数据已标记删除，直接return
            if (deleteType == DELETE_AUTO && block.isDelete())
            {
                return RT_DATA_DEL;
            }
            uint32_t iAddr = _pDataAllocator->relocateMemBlock(it->getAddr(), iVersion, block.getBlockHead()->_iMainKey, unHash%_hash.size(), bHead, iAllocSize, vtData);
            if (iAddr == 0)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }

            it = HashMapLockIterator(this, iAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);
        }
        else
        {
            // 数据尚不存在

            // 查找是否已经存在于主key链
            uint32_t mIndex = mhashIndex(mk);		// 主key索引
            uint32_t iMainKeyAddr = find(mk, mIndex, ret);
            if (ret != RT_OK && ret != RT_ONLY_KEY)
            {
                return ret;
            }
            if (iMainKeyAddr == 0)
            {
                // 主key头尚不存在，新建一个
                uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length());
                iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(mIndex, iAllocSize, vtData);
                if (iMainKeyAddr == 0)
                {
                    return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
                }
                // 将主key设置到主key头中
                MainKey mainKey(this, iMainKeyAddr);
                ret = mainKey.set(mk.c_str(), mk.length(), vtData);
                if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    doRecover();
                    return ret;
                }
            }

            // 先分配Blcok空间, 并获得淘汰的数据
            uint32_t iAddr = _pDataAllocator->allocateMemBlock(MainKey::HASH_TYPE, iMainKeyAddr, unHash%_hash.size(), bHead, iAllocSize, vtData);
            if (iAddr == 0)
            {
                doRecover();
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }

            it = HashMapLockIterator(this, iAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);
            iOldAddr = iAddr;
            bNewBlock = true;
        }

        bool bOldIsDelete = false;
        {
            Block block(this, iOldAddr);
            bOldIsDelete = block.isDelete();
        }

        ret = it->set(mk, uk, v, iExpireTime, iVersion, bCheckExpire, iNowTime, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            // set数据失败，可能的情况是空间不够了
            if (bNewBlock)
            {
                // 如果是新set的数据，此时要注意删除前面分配并挂接的block头
                // 否则这个block将永远无法访问，也不能删除，因为block里是一个空的block
                //Block block(this, it->getAddr());
                //block.erase();
                // modified by smitchzhao @ 2011-10-02
                doRecover();
            }
            else if (iOldAddr != it->getAddr())
            { //失败就要把新分配的块删除
                doRecover();
            }
            return ret;
        }
        else if (iOldAddr != it->getAddr())
        {//成功后，要把旧数据删除
            Block block(this, iOldAddr);
            if (block.isDelete())
            {
                // 把主key下数据个数增加一个，因为后面的block.erase()会把这个计数减一
                MainKey mainKey(this, block.getBlockHead()->_iMainKey);
                saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);
            }
            block.erase();
        }

        Block block(this, it->getAddr());
        if (bNewBlock)
        {
            block.setSyncTime(time(NULL));
        }
        block.setDirty(bDirty);

        if (deleteType != DELETE_AUTO)
        {
            block.setDelete(deleteType == DELETE_TRUE);
        }

        if (eType != AUTO_DATA)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.SETFULLDATA(eType == FULL_DATA);
        }

        block.refreshSetList();

        //如果不是新插入的，并且设置了更新重新排序
        // 或者之前是被删除的
        //就要重新更新key链下的顺序
        if ((!bNewBlock && bUpdateOrder) || (bOldIsDelete))
        {
            block.refreshKeyList(bHead);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::set(const string &mk, const string &uk, DATATYPE eType, bool bHead, bool bUpdateOrder, DELETETYPE deleteType, vector<Value> &vtData)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, ret);
        bool bNewBlock = false;

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        incHitCount();

        if (it != end())
        {
            if (deleteType != DELETE_AUTO)
            {
                Block block(this, it->getAddr());
                block.setDelete(deleteType == DELETE_TRUE);
            }

            // 主键下已经存在，可以不管，直接返回
            // 因为如果主键下已经有数据，是不能设置为only key的
            // 如果主键下没有数据，则必定已经是only key
            if (it->isOnlyKey())
            {
                return RT_OK;
            }
            return RT_DATA_EXIST;
        }

        // 查找是否已经存在于主key链
        uint32_t mIndex = mhashIndex(mk);		// 主key索引
        uint32_t iMainKeyAddr = find(mk, mIndex, ret);
        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key头尚不存在，新建一个
            uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length());
            iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(mIndex, iAllocSize, vtData);
            if (iMainKeyAddr == 0)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }
            // 将主key设置到主key头中
            MainKey mainKey(this, iMainKeyAddr);
            ret = mainKey.set(mk.c_str(), mk.length(), vtData);
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                doRecover();
                return ret;
            }
        }

        TC_PackIn pi;
        pi << uk;
        uint32_t iAllocSize = (uint32_t)(sizeof(Block::tagBlockHead) + pi.length());

        //先分配空间, 并获得淘汰的数据
        uint32_t iAddr = _pDataAllocator->allocateMemBlock(MainKey::HASH_TYPE, iMainKeyAddr, index, bHead, iAllocSize, vtData);
        if (iAddr == 0)
        {
            doRecover();
            return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
        }

        it = HashMapLockIterator(this, iAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);
        bNewBlock = true;

        ret = it->set(mk, uk, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            // set数据失败，可能的情况是空间不够了
            if (bNewBlock)
            {
                // 如果是新set的数据，此时要注意删除前面分配并挂接的block头
                // 否则这个block将永远无法访问，也不能删除，因为block里是一个空的block
                //Block block(this, it->getAddr());
                //block.erase();
                doRecover();
            }
            return ret;
        }

        Block block(this, it->getAddr());

        if (bNewBlock)
        {
            block.setSyncTime(time(NULL));
        }

        bool bIsDelete = block.isDelete();

        if (deleteType != DELETE_AUTO)
        {
            block.setDelete(deleteType == DELETE_TRUE);
        }

        if (eType != AUTO_DATA)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.SETFULLDATA(eType == FULL_DATA);
        }

        block.refreshSetList();

        //如果不是新插入的，并且设置了更新重新排序
        // 或者之前是被删除的
        //就要重新更新key链下的顺序
        if ((!bNewBlock && bUpdateOrder) || (bIsDelete))
        {
            block.refreshKeyList(bHead);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::set(const string &mk, vector<Value> &vtData)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        // 查找是否已经存在该主key
        uint32_t mIndex = mhashIndex(mk);		// 主key索引
        uint32_t iMainKeyAddr = find(mk, mIndex, ret);
        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }

        incHitCount();

        if (iMainKeyAddr != 0)
        {
            MainKey mainKey(this, iMainKeyAddr);

            // 主key已经存在，可以不管，直接返回
            // 如果主key有数据，是不能设为only key的
            // 而如果主key下没有数据，则必已经为only key 
            if (mainKey.getHeadPtr()->_iBlockHead != 0)
            {
                // 如果主key下已经存在数据，则不能设置为only key
                return RT_DATA_EXIST;
            }
            return RT_OK;
        }

        // 主key头尚不存在，新建一个
        uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length());
        iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(mIndex, iAllocSize, vtData);
        if (iMainKeyAddr == 0)
        {
            return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
        }

        MainKey mainKey(this, iMainKeyAddr);

        // 将主key设置到主key头中
        ret = mainKey.set(mk.c_str(), mk.length(), vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            doRecover();
            return ret;
        }
        mainKey.SETFULLDATA(true);

        mainKey.refreshGetList();

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::set(const vector<Value> &vtSet, DATATYPE eType, bool bHead, bool bUpdateOrder, bool bOrderByItem, bool bForce, vector<Value> &vtErased)
    {
        // 这里不需要加恢复代码
        //TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        int ret = RT_OK;

        for (size_t i = 0; i < vtSet.size(); i++)
        {
            if (vtSet[i]._ukey.empty())
            {
                // 没有联合key，当作主key的only key设置
                if (bForce || checkMainKey(vtSet[i]._mkey) == RT_NO_DATA)
                {
                    // 强制更新或主key下没有数据
                    ret = set(vtSet[i]._mkey, vtErased);
                }
                else
                {
                    if (eType != AUTO_DATA)
                    {
                        // 虽然不需要更新value，但要更新数据状态
                        setFullData(vtSet[i]._mkey, eType == FULL_DATA);
                    }
                }
            }
            else if (vtSet[i]._value.empty())
            {
                // 没有value，当作联合主键的only key设置
                if (bForce || find(vtSet[i]._mkey, vtSet[i]._ukey) == end())
                {
                    // 强制更新或联合主键下没有数据
                    ret = set(vtSet[i]._mkey, vtSet[i]._ukey, eType, bHead, bUpdateOrder, vtSet[i]._isDelete, vtErased);
                }
                else
                {
                    if (eType != AUTO_DATA)
                    {
                        // 虽然不需要更新value，但要更新数据状态
                        setFullData(vtSet[i]._mkey, eType == FULL_DATA);
                    }
                }
            }
            else
            {
                bool bExist = (find(vtSet[i]._mkey, vtSet[i]._ukey) != end());
                if (bForce || !bExist)
                {
                    // 强制更新或联合主键下没有数据
                    uint32_t unHash = getHashFunctor()(vtSet[i]._mkey + vtSet[i]._ukey);
                    ret = set(vtSet[i]._mkey, vtSet[i]._ukey, unHash, vtSet[i]._value,
                        vtSet[i]._iExpireTime, vtSet[i]._iVersion,
                        vtSet[i]._dirty, eType, (bOrderByItem) ? !bHead : bHead, bUpdateOrder, vtSet[i]._isDelete, vtErased);
                }
                else if (bOrderByItem && bExist && !bUpdateOrder)
                {//如果有数据，而且位置不调整的，就把数据的位置变更，保持和排序后的位置一致
                    uint32_t unHash = getHashFunctor()(vtSet[i]._mkey + vtSet[i]._ukey);
                    ret = refreshKeyList(vtSet[i]._mkey, vtSet[i]._ukey, unHash, !bHead);

                    if (eType != AUTO_DATA)
                    {
                        // 虽然不需要更新value，但要更新数据状态
                        setFullData(vtSet[i]._mkey, eType == FULL_DATA);
                    }
                }
                else
                {
                    if (eType != AUTO_DATA)
                    {
                        // 虽然不需要更新value，但要更新数据状态
                        setFullData(vtSet[i]._mkey, eType == FULL_DATA);
                    }
                }
            }
            if (ret != RT_OK && ret != RT_DATA_EXIST)
            {
                return ret;
            }
        }

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::update(const string &mk, const string &uk, const uint32_t unHash, const map<std::string, DCache::UpdateValue> &mpValue,
        const vector<DCache::Condition> & vtValueCond, const FieldConf *fieldInfo, bool bLimit, size_t iIndex, size_t iCount, std::string &retValue, bool bCheckExpire, uint32_t iNowTime, uint32_t iExpireTime,
        bool bDirty, DATATYPE eType, bool bHead, bool bUpdateOrder, vector<Value> &vtData)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        Value v;
        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        lock_iterator it = find(mk, uk, unHash%_hash.size(), v, ret);

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        Block block(this, it->getAddr());

        //已经标记为删除
        if (block.isDelete())
            return RT_DATA_DEL;

        incHitCount();

        // 如果需要检查数据是否已过期，数据过期时间不为0且已经小于iExpireTime的话，返回RT_DATE_EXPIRED
        if (bCheckExpire && v._iExpireTime != 0 && v._iExpireTime < iNowTime)
        {
            return RT_DATA_EXPIRED;
        }

        // 只有Key
        if (block.isOnlyKey())
        {
            return RT_ONLY_KEY;
        }

        // 如果只读, 则不刷新get链表
        if (!_pHead->_bReadOnly)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.refreshGetList();
        }

        HashMap::TarsDecode vDecode;

        vDecode.setBuffer(v._value);

        string value;

        try
        {
            if (HashMap::judge(vDecode, vtValueCond, *fieldInfo))
            {
                if (!bLimit || (bLimit && iIndex == 0 && iCount >= 1))
                {
                    value = HashMap::updateValue(mpValue, *fieldInfo, v._value);
                }
                else
                    return RT_NO_DATA;
            }
            else
                return RT_NO_DATA;
        }
        catch (exception &ex)
        {
            return RT_DECODE_ERR;
        }

        retValue = value;

        ret = it->set(mk, uk, value, iExpireTime, 0, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        block.setDirty(bDirty);

        bool bIsDelete = block.isDelete();

        if (eType != AUTO_DATA)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.SETFULLDATA(eType == FULL_DATA);
        }

        block.refreshSetList();

        //如果不是新插入的，并且设置了更新重新排序
        // 或者之前是被删除的
        //就要重新更新key链下的顺序
        if (bUpdateOrder || bIsDelete)
        {
            block.refreshKeyList(bHead);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::delSetBit(const string &mk, const string &uk, const time_t t)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int    ret = RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        incHitCount();

        Block block(this, it->getAddr());

        //如果已经标记为删除了，就返回RT_DATA_DEL
        if (block.isDelete())
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;

        //设置删除位
        block.setDelete(true);

        //复用同步时间字段，用于记录删除的时间
        block.getBlockHead()->_iSyncTime = t;

        //重置版本号
        block.getBlockHead()->_iVersion = 1;

        block.setDirty(false);

        return ret;
    }

    int TC_Multi_HashMap_Malloc::delSetBit(const string &mk, const string &uk, uint8_t iVersion, const time_t t)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int    ret = RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        incHitCount();

        Block block(this, it->getAddr());

        //如果已经标记为删除了，就返回RT_DATA_DEL
        if (block.isDelete())
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;

        if (ret == TC_Multi_HashMap_Malloc::RT_OK)
        {
            if ((iVersion != 0) && (iVersion != block.getBlockHead()->_iVersion))
                return TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH;
        }

        //设置删除位
        block.setDelete(true);

        //复用同步时间字段，用于记录删除的时间
        block.getBlockHead()->_iSyncTime = t;

        //重置版本号
        block.getBlockHead()->_iVersion = 1;

        block.setDirty(false);

        return ret;
    }

    int TC_Multi_HashMap_Malloc::delSetBit(const string &mk, const time_t t, uint64_t &delCount)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        delCount = 0;

        if (_pHead->_bReadOnly) return RT_READONLY;
        incGetCount();

        uint32_t iIndex = mhashIndex(mk);
        if (itemMainKey(iIndex)->_iMainKeyAddr != 0)
        {
            MainKey mainKey(this, itemMainKey(iIndex)->_iMainKeyAddr);
            do
            {
                string sKey;
                mainKey.get(sKey);
                if (mk == sKey)
                {
                    // 找到主key头了
                    uint32_t iAddr = mainKey.getHeadPtr()->_iBlockHead;

                    //如果mainKey下没数据了，就把mainKey也删除
                    if (0 == iAddr)
                    {
                        vector<Value> vtData;
                        return mainKey.erase(vtData);
                    }
                    while (iAddr != 0)
                    {
                        Block block(this, iAddr);

                        if (block.isDelete() == false)
                        {
                            _pstCurrModify = _pstInnerModify;
                            //设置删除标志
                            block.setDelete(true);

                            //复用同步时间，用于记录删除时间
                            saveValue(&(block.getBlockHead()->_iSyncTime), (uint32_t)t);

                            //重置版本号
                            saveValue(&(block.getBlockHead()->_iVersion), (uint8_t)1);

                            block.setDirty(false);

                            //这一条设置完毕，可以清理保护区
                            doUpdate();

                            _pstCurrModify = _pstOuterModify;

                            delCount++;
                        }

                        iAddr = block.getBlockHead()->_iMKBlockNext;
                    }

                    //表示有新数据标明删除
                    if (delCount != 0)
                    {
                        incHitCount();

                        return TC_Multi_HashMap_Malloc::RT_OK;
                    }
                    else//表示所有数据都已经标明是删除的了
                        return TC_Multi_HashMap_Malloc::RT_DATA_DEL;
                }
            } while (mainKey.next());
        }

        return TC_Multi_HashMap_Malloc::RT_NO_DATA;
    }

    int TC_Multi_HashMap_Malloc::delReal(const string &mk, const string &uk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        if (_pHead->_bReadOnly) return RT_READONLY;

        int    ret = RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        Block block(this, it->getAddr());

        //如果是标记为删除的，就可以删除了
        if (block.isDelete())
        {
            // 把主key下数据个数增加一个，因为后面的block.erase()会把这个计数减一
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);

            block.erase(false);

            //如果mainKey下没数据了，就把mainKey也删除
            if (mainKey.getHeadPtr()->_iBlockHead == 0)
            {
                vector<Value> vtData;
                mainKey.erase(vtData);
            }
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::delForce(const string &mk, const string &uk, Value &data)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int    ret = RT_OK;
        uint32_t index = hashIndex(mk, uk);
        lock_iterator it = find(mk, uk, index, data, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        incHitCount();

        Block block(this, it->getAddr());

        if (block.isDelete())
        {
            // 把主key下数据个数增加一个，因为后面的block.erase()会把这个计数减一
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);
        }

        block.erase(false);

        //如果mainKey下没数据了，就把mainKey也删除
        MainKey mainKey(this, block.getBlockHead()->_iMainKey);
        if (mainKey.getHeadPtr()->_iBlockHead == 0)
        {
            vector<Value> vtData;
            mainKey.erase(vtData);
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::delForce(const string &mk, vector<Value> &data)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);

        if (_pHead->_bReadOnly) return RT_READONLY;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        uint32_t iIndex = mhashIndex(mk);
        if (itemMainKey(iIndex)->_iMainKeyAddr != 0)
        {
            MainKey mainKey(this, itemMainKey(iIndex)->_iMainKeyAddr);
            do
            {
                string sKey;
                if (MainKey::ZSET_TYPE != keyType)
                    mainKey.get(sKey);
                else
                    mainKey.getData(sKey);
                if (mk == sKey)
                {
                    // 找到主key头了
                    incHitCount();
                    return mainKey.erase(data);
                }
            } while (mainKey.next());
        }

        return TC_Multi_HashMap_Malloc::RT_NO_DATA;
    }

    int TC_Multi_HashMap_Malloc::del(const string &mk, const string &uk, Value &data)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return RT_EXCEPTION_ERR;

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int    ret = RT_OK;
        uint32_t index = hashIndex(mk, uk);
        Value tmpData;
        lock_iterator it = find(mk, uk, index, tmpData, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        Block block(this, it->getAddr());

        //如果是标记为删除。就直接返回
        if (block.isDelete())
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;

        incHitCount();

        block.erase(false);

        //如果mainKey下没数据了，就把mainKey也删除
        MainKey mainKey(this, block.getBlockHead()->_iMainKey);
        if (mainKey.getHeadPtr()->_iBlockHead == 0)
        {
            vector<Value> vtData;
            mainKey.erase(vtData);
        }

        data = tmpData;

        return ret;
    }

    int TC_Multi_HashMap_Malloc::del(const string &mk, vector<Value> &data)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);

        if (_pHead->_bReadOnly) return RT_READONLY;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        uint32_t iIndex = mhashIndex(mk);
        if (itemMainKey(iIndex)->_iMainKeyAddr != 0)
        {
            MainKey mainKey(this, itemMainKey(iIndex)->_iMainKeyAddr);
            do
            {
                string sKey;
                if (keyType != MainKey::ZSET_TYPE)
                    mainKey.get(sKey);
                else
                    mainKey.getData(sKey);
                if (mk == sKey)
                {
                    // 找到主key头了
                    incHitCount();

                    int deleteCount = 0;
                    uint32_t iAddr = mainKey.getHeadPtr()->_iBlockHead;
                    if (keyType != MainKey::ZSET_TYPE && iAddr == 0)
                    {
                        return mainKey.erase(data);
                    }

                    if (keyType == MainKey::HASH_TYPE)
                    {
                        while (iAddr != 0)
                        {
                            Block block(this, iAddr);
                            iAddr = block.getBlockHead()->_iMKBlockNext;

                            if (block.isDelete() == false)
                            {
                                BlockData blkData;
                                int ret = block.getBlockData(blkData);
                                if (ret != RT_OK && ret != RT_ONLY_KEY)
                                {
                                    return ret;
                                }
                                Value v(mk, blkData);
                                data.push_back(v);

                                block.erase(false);

                                deleteCount++;
                            }
                        }
                    }
                    else if (keyType == MainKey::LIST_TYPE)
                    {
                        while (iAddr != 0)
                        {
                            Block block(this, iAddr);
                            iAddr = block.getListBlockHead()->_iMKBlockNext;

                            BlockData blkData;
                            int ret = block.getListBlockData(blkData);
                            if (ret != RT_OK && ret != RT_ONLY_KEY)
                            {
                                return ret;
                            }
                            Value v(mk, blkData);
                            data.push_back(v);

                            block.eraseList(false);

                            deleteCount++;
                        }
                    }
                    else if (keyType == MainKey::SET_TYPE)
                    {
                        while (iAddr != 0)
                        {
                            Block block(this, iAddr);
                            iAddr = block.getBlockHead()->_iMKBlockNext;

                            if (block.isDelete() == false)
                            {
                                BlockData blkData;
                                int ret = block.getSetBlockData(blkData);
                                if (ret != RT_OK && ret != RT_ONLY_KEY)
                                {
                                    return ret;
                                }
                                Value v(mk, blkData);
                                data.push_back(v);

                                block.erase(false);

                                deleteCount++;
                            }
                        }
                    }
                    else if (keyType == MainKey::ZSET_TYPE)
                    {
                        iAddr = mainKey.getNext(0);
                        while (iAddr != 0)
                        {
                            Block block(this, iAddr);
                            iAddr = block.getNext(0);

                            if (block.isDelete() == false)
                            {
                                BlockData blkData;
                                int ret = block.getZSetBlockData(blkData);
                                if (ret != RT_OK && ret != RT_ONLY_KEY)
                                {
                                    return ret;
                                }
                                Value v(mk, blkData);
                                data.push_back(v);

                                //delSkipList(mainKey, block.getHead());
                                block.eraseZSet(false);

                                deleteCount++;
                            }
                        }
                    }

                    // 把主key下数据个数设为0
                    saveValue(&mainKey.getHeadPtr()->_iBlockCount, 0);

                    //表示有新数据标明删除
                    if (deleteCount != 0)
                    {
                        //如果mainKey下没数据了，就把mainKey也删除
                        if ((keyType != MainKey::ZSET_TYPE && mainKey.getHeadPtr()->_iBlockHead == 0)
                            || (keyType == MainKey::ZSET_TYPE && mainKey.getNext(0) == 0))
                        {
                            vector<Value> vtData;
                            mainKey.erase(vtData);
                        }
                        incHitCount();
                        return TC_Multi_HashMap_Malloc::RT_OK;
                    }
                    else//表示所有数据都已经标明是删除的了
                        return TC_Multi_HashMap_Malloc::RT_DATA_DEL;

                }
            } while (mainKey.next());
        }

        return TC_Multi_HashMap_Malloc::RT_NO_DATA;
    }

    int TC_Multi_HashMap_Malloc::del(const string &mk, size_t iCount, vector<Value> &data,
        size_t iStart /* = 0 */, bool bHead /* = true */, bool bDelDirty /* false*/)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        if (_pHead->_bReadOnly) return RT_READONLY;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        int ret = RT_OK;

        incGetCount();

        uint32_t iIndex = mhashIndex(mk);
        uint32_t iMainKeyAddr = itemMainKey(iIndex)->_iMainKeyAddr;
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        MainKey mainKey(this, iMainKeyAddr);
        do
        {
            string sKey;
            mainKey.get(sKey);
            if (mk == sKey)
            {
                // 找到主key头了
                incHitCount();
                if (mainKey.getHeadPtr()->_iBlockHead == 0)
                {
                    return RT_ONLY_KEY;
                }
                //if(iStart == 0 && iCount >= mainKey.getHeadPtr()->_iBlockCount)
                //{
                    // 要删除的数量比主key下的数据要多，直接将整个主key删除
                    //return mainKey.erase(data);
                //}
                uint32_t iAddr = 0;
                if (bHead)
                {
                    iAddr = mainKey.getHeadPtr()->_iBlockHead;
                }
                else
                {
                    iAddr = mainKey.getHeadPtr()->_iBlockTail;
                }
                size_t iIndex = 0, iDelCount = 0;
                vector<uint32_t> vtAddrs;
                while (iAddr != 0 && iDelCount < iCount)
                {
                    Block block(this, iAddr);

                    //如果是脏数据，就立即退出
                    if (!bDelDirty && block.isDirty())
                        break;

                    if ((block.isDelete() == false) && (iIndex++ >= iStart))
                    {
                        BlockData blkData;
                        ret = block.getBlockData(blkData);
                        if (ret != RT_OK && ret != RT_ONLY_KEY)
                        {
                            return ret;
                        }
                        Value v(mk, blkData);
                        vtAddrs.push_back(iAddr);
                        data.push_back(v);
                        iDelCount++;
                    }
                    if (bHead)
                    {
                        iAddr = block.getBlockHead()->_iMKBlockNext;
                    }
                    else
                    {
                        iAddr = block.getBlockHead()->_iMKBlockPrev;
                    }
                }
                // 取完数据后再逐个删除
                for (size_t i = 0; i < vtAddrs.size(); i++)
                {
                    Block block(this, vtAddrs[i]);
                    block.erase(false);
                }
                //如果mainKey下没数据了，就把mainKey也删除
                if (mainKey.getHeadPtr()->_iBlockHead == 0)
                {
                    vector<Value> vtData;
                    mainKey.erase(vtData);
                }
                break;
            }
        } while (mainKey.next());

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::erase(int ratio, vector<Value> &vtData, bool bCheckDirty)
    {
        if (_pHead->_bReadOnly) return RT_READONLY;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);

        if (ratio <= 0)   ratio = 1;
        if (ratio >= 100) ratio = 100;

        uint32_t iAddr = _pHead->_iGetTail;

        //到链表头部
        if (iAddr == 0)
        {
            return RT_OK;
        }

        //删除到指定比率了
        if (int(_pHead->_iUsedDataMem * 100 / getDataMemSize()) < ratio)
        {
            if ((!(_iMainKeySize > 0)) || (int(_pHead->_iMKUsedMem * 100 / getMainKeyMemSize()) < ratio))
            {
                return RT_OK;
            }
        }

        // 删除是针对主key的
        MainKey mainKey(this, iAddr);
        if ((keyType == MainKey::HASH_TYPE || keyType == MainKey::SET_TYPE || keyType == MainKey::ZSET_TYPE) && bCheckDirty)
        {
            if (keyType == MainKey::ZSET_TYPE)
            {
                uint32_t iBlockAddr = mainKey.getNext(0);
                while (iBlockAddr != 0)
                {
                    Block block(this, iBlockAddr);
                    iBlockAddr = block.getNext(0);

                    if (block.isDelete() || block.isDirty())
                    {
                        if (_pHead->_iDirtyTail == 0)
                        {
                            _pHead->_iDirtyTail = mainKey.getHeadPtr()->_iBlockHead;
                        }

                        // 将脏数据移动到get链的头部，使可以继续淘汰
                        if (_pHead->_iGetHead == iAddr)
                        {
                            // 既是头也是尾，只有一个元素了
                            return RT_OK;
                        }

                        if (_iReadP == iAddr)
                        {
                            _iReadP = mainKey.getHeadPtr()->_iGetPrev;
                        }

                        // 将GetTail上移
                        saveValue(&_pHead->_iGetTail, mainKey.getHeadPtr()->_iGetPrev);
                        // 从Get尾部移除
                        saveValue(&mainKey.getHeadPtr(mainKey.getHeadPtr()->_iGetPrev)->_iGetNext, (uint32_t)0);
                        saveValue(&mainKey.getHeadPtr()->_iGetPrev, (uint32_t)0);
                        // 移到Get头部
                        saveValue(&mainKey.getHeadPtr()->_iGetNext, _pHead->_iGetHead);
                        saveValue(&mainKey.getHeadPtr(_pHead->_iGetHead)->_iGetPrev, iAddr);
                        saveValue(&_pHead->_iGetHead, iAddr);

                        return RT_DIRTY_DATA;
                    }
                }
            }
            else
            {
                // 检查脏数据，脏数据不淘汰
                lock_iterator it(this, mainKey.getHeadPtr()->_iBlockHead, lock_iterator::IT_MKEY, lock_iterator::IT_NEXT);
                while (it != end())
                {
                    //如果主key下的数据都标记为删除，则返回RT_DIRTY_DATA
                    if ((mainKey.getHeadPtr()->_iBlockCount == 0) || (it->isDirty()))
                    {
                        if (_pHead->_iDirtyTail == 0)
                        {
                            _pHead->_iDirtyTail = mainKey.getHeadPtr()->_iBlockHead;
                        }

                        // 将脏数据移动到get链的头部，使可以继续淘汰
                        if (_pHead->_iGetHead == iAddr)
                        {
                            // 既是头也是尾，只有一个元素了
                            return RT_OK;
                        }

                        if (_iReadP == iAddr)
                        {
                            _iReadP = mainKey.getHeadPtr()->_iGetPrev;
                        }

                        // 将GetTail上移
                        saveValue(&_pHead->_iGetTail, mainKey.getHeadPtr()->_iGetPrev);
                        // 从Get尾部移除
                        saveValue(&mainKey.getHeadPtr(mainKey.getHeadPtr()->_iGetPrev)->_iGetNext, (uint32_t)0);
                        saveValue(&mainKey.getHeadPtr()->_iGetPrev, (uint32_t)0);
                        // 移到Get头部
                        saveValue(&mainKey.getHeadPtr()->_iGetNext, _pHead->_iGetHead);
                        saveValue(&mainKey.getHeadPtr(_pHead->_iGetHead)->_iGetPrev, iAddr);
                        saveValue(&_pHead->_iGetHead, iAddr);

                        return RT_DIRTY_DATA;
                    }
                    it++;
                }
            }
        }

        // 淘汰主key下的所有数据
        //mainKey.erase(vtData);
        string mKey;
        if (keyType != MainKey::ZSET_TYPE)
            mainKey.get(mKey);
        else
            mainKey.getData(mKey);

        if (keyType == MainKey::HASH_TYPE)
        {
            uint32_t iBlockAddr = mainKey.getHeadPtr()->_iBlockHead;
            while (iBlockAddr != 0)
            {
                Block block(this, iBlockAddr);
                iBlockAddr = block.getBlockHead()->_iMKBlockNext;

                if (block.isDelete() == false)
                {
                    BlockData blkData;
                    int ret = block.getBlockData(blkData);
                    if (ret == RT_OK)
                    {
                        Value v(mKey, blkData);
                        vtData.push_back(v);
                    }

                    block.erase(false);
                }
            }
        }
        else if (keyType == MainKey::LIST_TYPE)
        {
            uint32_t iBlockAddr = mainKey.getHeadPtr()->_iBlockHead;
            while (iBlockAddr != 0)
            {
                Block block(this, iBlockAddr);
                iBlockAddr = block.getListBlockHead()->_iMKBlockNext;

                BlockData blkData;
                int ret = block.getListBlockData(blkData);
                if (ret == RT_OK)
                {
                    Value v(mKey, blkData);
                    vtData.push_back(v);
                }

                block.eraseList(false);
            }
        }
        else if (keyType == MainKey::SET_TYPE)
        {
            uint32_t iBlockAddr = mainKey.getHeadPtr()->_iBlockHead;
            while (iBlockAddr != 0)
            {
                Block block(this, iBlockAddr);
                iBlockAddr = block.getBlockHead()->_iMKBlockNext;

                if (block.isDelete() == false)
                {
                    BlockData blkData;
                    int ret = block.getSetBlockData(blkData);
                    if (ret == RT_OK)
                    {
                        Value v(mKey, blkData);
                        vtData.push_back(v);
                    }

                    block.erase(false);
                }
            }
        }
        else if (keyType == MainKey::ZSET_TYPE)
        {
            uint32_t iBlockAddr = mainKey.getNext(0);
            while (iBlockAddr != 0)
            {
                Block block(this, iBlockAddr);
                iBlockAddr = block.getNext(0);

                if (block.isDelete() == false)
                {
                    BlockData blkData;
                    int ret = block.getSetBlockData(blkData);
                    if (ret == RT_OK)
                    {
                        Value v(mKey, blkData);
                        vtData.push_back(v);
                    }

                    //delSkipList(mainKey, block.getHead());
                    block.eraseZSet(false);
                }
            }
        }

        //如果主key下所有数据都删除，就把主key头也干掉
        if ((keyType != MainKey::ZSET_TYPE) && (mainKey.getHeadPtr()->_iBlockHead == 0))
        {
            mainKey.erase(vtData);
        }
        else if ((keyType == MainKey::ZSET_TYPE) && (0 == mainKey.getNext(0)))
        {
            mainKey.erase(vtData);
        }
        else
        {//如果不是，就把数据移到get链的头部
            if (_pHead->_iDirtyTail == 0)
            {
                _pHead->_iDirtyTail = mainKey.getHeadPtr()->_iBlockHead;
            }

            // 将脏数据移动到get链的头部，使可以继续淘汰
            if (_pHead->_iGetHead == iAddr)
            {
                // 既是头也是尾，只有一个元素了
                return RT_ERASE_OK;
            }

            if (_iReadP == iAddr)
            {
                _iReadP = mainKey.getHeadPtr()->_iGetPrev;
            }

            // 将GetTail上移
            saveValue(&_pHead->_iGetTail, mainKey.getHeadPtr()->_iGetPrev);
            // 从Get尾部移除
            saveValue(&mainKey.getHeadPtr(mainKey.getHeadPtr()->_iGetPrev)->_iGetNext, (uint32_t)0);
            saveValue(&mainKey.getHeadPtr()->_iGetPrev, (uint32_t)0);
            // 移到Get头部
            saveValue(&mainKey.getHeadPtr()->_iGetNext, _pHead->_iGetHead);
            saveValue(&mainKey.getHeadPtr(_pHead->_iGetHead)->_iGetPrev, iAddr);
            saveValue(&_pHead->_iGetHead, iAddr);

            //把主key设为非全量数据
            mainKey.SETFULLDATA(false);
        }

        return RT_ERASE_OK;
    }

    int TC_Multi_HashMap_Malloc::pushList(const string &mk, const vector<pair<uint32_t, string> > &vt, const bool bHead, const bool bReplace, const uint64_t iPos, const uint32_t iNowTime, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::LIST_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t mkHash = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, mkHash, ret);
        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            if (bReplace)
            {
                return RT_NO_DATA;
            }

            // 主key头尚不存在，新建一个
            uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length());
            iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(mkHash, iAllocSize, vtData);
            if (iMainKeyAddr == 0)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }
            // 将主key设置到主key头中
            MainKey mainKey(this, iMainKeyAddr);
            ret = mainKey.set(mk.c_str(), mk.length(), vtData);
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                doRecover();
                return ret;
            }
        }

        for (unsigned int i = 0; i < vt.size(); i++)
        {
            const string &v = vt[i].second;
            if (!bReplace)
            {
                uint32_t iAllocSize = (uint32_t)(sizeof(Block::tagListBlockHead) + v.size());

                // 先分配Blcok空间, 并获得淘汰的数据
                uint32_t iAddr = _pDataAllocator->allocateMemBlock(MainKey::LIST_TYPE, iMainKeyAddr, 0, bHead, iAllocSize, vtData);
                if (iAddr == 0)
                {
                    doRecover();
                    return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
                }

                lock_iterator it = HashMapLockIterator(this, iAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);

                ret = it->setListBlock(mk, v, vt[i].first, vtData);
                if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    // set数据失败，可能的情况是空间不够了
                    //if(bNewBlock)
                    {
                        // 如果是新set的数据，此时要注意删除前面分配并挂接的block头
                        // 否则这个block将永远无法访问，也不能删除，因为block里是一个空的block
                        //Block block(this, it->getAddr());
                        //block.erase();
                        // modified by smitchzhao @ 2011-10-02
                        doRecover();
                    }
                    return ret;
                }
            }
            else
            {
                MainKey mainKey(this, iMainKeyAddr);

                uint32_t iAddr = mainKey.getHeadPtr()->_iBlockHead;

                uint64_t iIndex = 0;
                while (iAddr != 0)
                {
                    Block block(this, iAddr);
                    if (iNowTime == 0 || block.getListBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getListBlockHead()->_iExpireTime > iNowTime))
                    {
                        if (iPos != iIndex)
                        {
                            iIndex++;
                        }
                        else
                        {
                            lock_iterator it = HashMapLockIterator(this, iAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);

                            ret = it->setListBlock(mk, v, vt[i].first, vtData);
                            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                            {
                                // set数据失败，可能的情况是空间不够了
                                //if(bNewBlock)
                                {
                                    // 如果是新set的数据，此时要注意删除前面分配并挂接的block头
                                    // 否则这个block将永远无法访问，也不能删除，因为block里是一个空的block
                                    //Block block(this, it->getAddr());
                                    //block.erase();
                                    // modified by smitchzhao @ 2011-10-02
                                    doRecover();
                                }
                                return ret;
                            }

                            return RT_OK;
                        }
                    }

                    iAddr = block.getListBlockHead()->_iMKBlockNext;
                }

                return TC_Multi_HashMap_Malloc::RT_NO_DATA;
            }

            //这一条设置完毕，可以清理保护区
            doUpdate();
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::pushList(const string &mk, const vector<Value> &vt, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::LIST_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        vector<pair<uint32_t, string> > vValue;
        for (unsigned int i = 0; i < vt.size(); i++)
        {
            vValue.clear();
            vValue.push_back(make_pair(vt[i]._iExpireTime, vt[i]._value));
            int iRet = pushList(mk, vValue, false, false, 0, 0, vtData);
            if (iRet != RT_OK)
                return iRet;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::getList(const string &mk, const uint64_t iStart, const uint64_t iEnd, const uint32_t iNowTime, vector<string> &vs)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::LIST_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t mkHash = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, mkHash, ret);
        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            return RT_NO_DATA;
        }

        MainKey mainKey(this, iMainKeyAddr);

        uint32_t iAddr = mainKey.getHeadPtr()->_iBlockHead;

        uint32_t iBlockCount = mainKey.getHeadPtr()->_iBlockCount;
        if ((iEnd - iStart) > iBlockCount)
            vs.reserve(iBlockCount);
        else
            vs.reserve(iEnd - iStart);

        size_t iIndex = 0;
        while (iAddr != 0 && iIndex <= iEnd)
        {
            Block block(this, iAddr);
            if (iNowTime == 0 || block.getListBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getListBlockHead()->_iExpireTime > iNowTime))
            {
                if (iIndex++ >= iStart)
                {
                    BlockData data;
                    ret = block.getListBlockData(data);
                    if (ret != RT_OK && ret != RT_ONLY_KEY)
                    {
                        return ret;
                    }
                    if (ret == RT_OK)
                    {
                        vs.push_back(data._value);
                    }
                }
            }

            iAddr = block.getListBlockHead()->_iMKBlockNext;
        }

        // 如果只读, 则不刷新get链表
        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::trimList(const string &mk, const bool bPop, const bool bHead, const bool bTrim, const uint64_t iStart, const uint64_t iCount, const uint32_t iNowTime, string &value, uint64_t &delSize)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::LIST_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        if (_pHead->_bReadOnly) return RT_READONLY;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        vector<Value> data;

        incGetCount();

        delSize = 0;

        uint32_t iIndex = mhashIndex(mk);
        if (itemMainKey(iIndex)->_iMainKeyAddr != 0)
        {
            MainKey mainKey(this, itemMainKey(iIndex)->_iMainKeyAddr);
            do
            {
                string sKey;
                mainKey.get(sKey);
                if (mk == sKey)
                {
                    // 找到主key头了
                    incHitCount();
                    mainKey.refreshGetList();

                    if (bPop)
                    {//只取出一条数据，然后删掉
                        uint32_t iAddr;
                        if (bHead)
                            iAddr = mainKey.getHeadPtr()->_iBlockHead;
                        else
                            iAddr = mainKey.getHeadPtr()->_iBlockTail;

                        while (iAddr != 0)
                        {
                            Block block(this, iAddr);
                            if (iNowTime == 0 || block.getListBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getListBlockHead()->_iExpireTime > iNowTime))
                                break;
                            else
                            {
                                if (bHead)
                                    iAddr = block.getListBlockHead()->_iMKBlockNext;
                                else
                                    iAddr = block.getListBlockHead()->_iMKBlockPrev;
                            }
                        }

                        if (iAddr == 0)
                        {
                            return RT_ONLY_KEY;
                        }
                        else
                        {
                            Block block(this, iAddr);
                            BlockData blkData;
                            int ret = block.getListBlockData(blkData);
                            if (ret != RT_OK && ret != RT_ONLY_KEY)
                            {
                                return ret;
                            }
                            else
                            {
                                value = blkData._value;
                                block.eraseList(true);
                                delSize = 1;
                                return TC_Multi_HashMap_Malloc::RT_OK;
                            }
                        }
                    }
                    else if (bTrim)
                    {//修剪范围外的数据
                        uint64_t iEnd = iCount;

                        uint32_t iAddr = mainKey.getHeadPtr()->_iBlockHead;
                        if (iAddr == 0)
                        {
                            return RT_ONLY_KEY;
                        }

                        uint64_t index = 0;
                        while (iAddr != 0)
                        {
                            Block block(this, iAddr);
                            iAddr = block.getListBlockHead()->_iMKBlockNext;

                            if (iNowTime == 0 || block.getListBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getListBlockHead()->_iExpireTime > iNowTime))
                            {
                                if (index < iStart || index > iEnd)
                                {
                                    block.eraseList(true);
                                    delSize++;
                                }
                            }

                            index++;
                        }
                    }
                    else
                    {//删除指定个数的数据
                        unsigned int deleteCount = 0;
                        if (bHead)
                        {
                            uint32_t iAddr = mainKey.getHeadPtr()->_iBlockHead;
                            if (iAddr == 0)
                            {
                                return RT_ONLY_KEY;
                            }

                            for (unsigned int i = 0; deleteCount < iCount && iAddr != 0; i++)
                            {
                                Block block(this, iAddr);
                                iAddr = block.getListBlockHead()->_iMKBlockNext;

                                if (iNowTime == 0 || block.getListBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getListBlockHead()->_iExpireTime > iNowTime))
                                {
                                    block.eraseList(true);
                                    delSize++;

                                    deleteCount++;
                                }
                            }

                        }
                        else
                        {
                            uint32_t iAddr = mainKey.getHeadPtr()->_iBlockTail;
                            if (iAddr == 0)
                            {
                                return RT_ONLY_KEY;
                            }

                            for (unsigned int i = 0; deleteCount < iCount && iAddr != 0; i++)
                            {
                                Block block(this, iAddr);
                                iAddr = block.getListBlockHead()->_iMKBlockPrev;

                                if (iNowTime == 0 || block.getListBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getListBlockHead()->_iExpireTime > iNowTime))
                                {
                                    block.eraseList(true);
                                    delSize++;

                                    deleteCount++;
                                }
                            }
                        }
                    }
                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
            } while (mainKey.next());
        }

        return TC_Multi_HashMap_Malloc::RT_NO_DATA;
    }

    int TC_Multi_HashMap_Malloc::addSet(const string &mk, const string &v, const uint32_t unHash, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, DELETETYPE deleteType, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::SET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        lock_iterator it = findSet(mk, v, unHash%_hash.size(), ret);
        bool bNewBlock = false;

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        incHitCount();

        uint32_t iMainKeyAddr;

        if (it == end())
        {
            // 数据尚不存在

            // 查找是否已经存在于主key链
            uint32_t mIndex = mhashIndex(mk);		// 主key索引
            iMainKeyAddr = find(mk, mIndex, ret);
            if (ret != RT_OK && ret != RT_ONLY_KEY)
            {
                return ret;
            }
            if (iMainKeyAddr == 0)
            {
                // 主key头尚不存在，新建一个
                uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length());
                iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(mIndex, iAllocSize, vtData);
                if (iMainKeyAddr == 0)
                {
                    return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
                }
                // 将主key设置到主key头中
                MainKey mainKey(this, iMainKeyAddr);
                ret = mainKey.set(mk.c_str(), mk.length(), vtData);
                if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    doRecover();
                    return ret;
                }
            }

            uint32_t iAllocSize = (uint32_t)(sizeof(Block::tagBlockHead) + v.length());

            // 先分配Blcok空间, 并获得淘汰的数据
            uint32_t iAddr = _pDataAllocator->allocateMemBlock(MainKey::SET_TYPE, iMainKeyAddr, unHash%_hash.size(), true, iAllocSize, vtData);
            if (iAddr == 0)
            {
                doRecover();
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }

            it = HashMapLockIterator(this, iAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);
            bNewBlock = true;
        }
        else
        {
            if (deleteType == DELETE_AUTO)
                return RT_OK;
        }

        ret = it->set(mk, v, iExpireTime, iVersion, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            // set数据失败，可能的情况是空间不够了
            if (bNewBlock)
            {
                // 如果是新set的数据，此时要注意删除前面分配并挂接的block头
                // 否则这个block将永远无法访问，也不能删除，因为block里是一个空的block
                //Block block(this, it->getAddr());
                //block.erase();
                // modified by smitchzhao @ 2011-10-02
                doRecover();
            }
            return ret;
        }

        Block block(this, it->getAddr());
        if (bNewBlock)
        {
            block.setSyncTime(time(NULL));
        }
        block.setDirty(bDirty);

        if (deleteType != DELETE_AUTO)
        {
            block.setDelete(deleteType == DELETE_TRUE);
        }

        /*if(eType != AUTO_DATA)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.SETFULLDATA(eType == FULL_DATA);
        }*/

        block.refreshSetList();

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::addSet(const string &mk, const vector<Value> &vt, DATATYPE eType, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::SET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        for (unsigned int i = 0; i < vt.size(); i++)
        {
            uint32_t unHash = getHashFunctor()(mk + vt[i]._value);
            int iRet = addSet(mk, vt[i]._value, unHash, vt[i]._iExpireTime, vt[i]._iVersion, vt[i]._dirty, vt[i]._isDelete, vtData);
            if (iRet != RT_OK && iRet != RT_DATA_EXIST)
                return iRet;
        }

        if (eType != AUTO_DATA)
        {
            // 虽然不需要更新value，但要更新数据状态
            setFullData(mk, eType == FULL_DATA);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::addSet(const string &mk, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::SET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key头尚不存在，新建一个
            uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length());
            iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(index, iAllocSize, vtData);
            if (iMainKeyAddr == 0)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }
            // 将主key设置到主key头中
            MainKey mainKey(this, iMainKeyAddr);
            ret = mainKey.set(mk.c_str(), mk.length(), vtData);
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                doRecover();
                return ret;
            }

            mainKey.SETFULLDATA(true);

            mainKey.refreshGetList();
        }

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::getSet(const string &mk, const uint32_t iNowTime, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::SET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        incHitCount();

        MainKey mainKey(this, iMainKeyAddr);
        if (mainKey.getHeadPtr()->_iBlockHead == 0)
        {
            return RT_ONLY_KEY;
        }
        uint32_t iAddr = mainKey.getHeadPtr()->_iBlockHead;

        uint32_t iBlockCount = mainKey.getHeadPtr()->_iBlockCount;
        vtData.reserve(iBlockCount);

        while (iAddr != 0)
        {
            Block block(this, iAddr);
            if (block.isDelete() == false && (iNowTime == 0 || block.getBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getBlockHead()->_iExpireTime > iNowTime)))
            {
                BlockData data;
                ret = block.getSetBlockData(data);
                if (ret != RT_OK && ret != RT_ONLY_KEY)
                {
                    return ret;
                }
                if (ret == RT_OK)
                {
                    // 仅取非only key的数据
                    Value v(mk, data);
                    v._keyType = MainKey::SET_TYPE;
                    vtData.push_back(v);
                }
            }

            iAddr = block.getBlockHead()->_iMKBlockNext;
        }

        //修复老版本 get完以后没有刷新get链
        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        if (!mainKey.ISFULLDATA())
        {
            return RT_PART_DATA;
        }

        if (vtData.size() == 0)
        {
            return RT_NO_DATA;
        }

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::delSetSetBit(const string &mk, const string &v, const uint32_t unHash, const time_t t)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::SET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int    ret = RT_OK;
        uint32_t index = hashIndex(mk, v);
        Value tmpData;
        lock_iterator it = findSet(mk, v, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        Block block(this, it->getAddr());

        //如果是标记为删除。就直接返回
        if (block.isDelete())
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;

        incHitCount();

        //设置删除位
        block.setDelete(true);

        //复用同步时间字段，用于记录删除的时间
        block.getBlockHead()->_iSyncTime = t;

        //重置版本号
        block.getBlockHead()->_iVersion = 1;

        block.setDirty(false);

        block.setExpireTime(0);

        return ret;
    }

    int TC_Multi_HashMap_Malloc::delSetReal(const string &mk, const string &v, const uint32_t unHash)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::SET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int    ret = RT_OK;
        uint32_t index = hashIndex(mk, v);
        Value tmpData;
        lock_iterator it = findSet(mk, v, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        Block block(this, it->getAddr());

        //如果是标记为删除。就可以删除
        if (block.isDelete())
        {
            // 把主key下数据个数增加一个，因为后面的block.erase()会把这个计数减一
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);

            block.erase(false);

            //如果mainKey下没数据了，就把mainKey也删除
            if (mainKey.getHeadPtr()->_iBlockHead == 0)
            {
                vector<Value> vtData;
                mainKey.erase(vtData);
            }
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::delSetSetBit(const string &mk, const time_t t)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::SET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        if (_pHead->_bReadOnly) return RT_READONLY;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        uint32_t iIndex = mhashIndex(mk);
        if (itemMainKey(iIndex)->_iMainKeyAddr != 0)
        {
            MainKey mainKey(this, itemMainKey(iIndex)->_iMainKeyAddr);
            do
            {
                string sKey;
                mainKey.get(sKey);
                if (mk == sKey)
                {
                    // 找到主key头了
                    incHitCount();

                    int deleteCount = 0;
                    uint32_t iAddr = mainKey.getHeadPtr()->_iBlockHead;
                    if (iAddr == 0)
                    {
                        vector<Value> vtData;
                        return mainKey.erase(vtData);
                    }
                    while (iAddr != 0)
                    {
                        Block block(this, iAddr);
                        iAddr = block.getBlockHead()->_iMKBlockNext;

                        if (block.isDelete() == false)
                        {
                            _pstCurrModify = _pstInnerModify;
                            //设置删除标志
                            block.setDelete(true);

                            //复用同步时间，用于记录删除时间
                            saveValue(&(block.getBlockHead()->_iSyncTime), (uint32_t)t);

                            //重置版本号
                            saveValue(&(block.getBlockHead()->_iVersion), (uint8_t)1);

                            block.setDirty(false);

                            block.setExpireTime(0);

                            //这一条设置完毕，可以清理保护区
                            doUpdate();

                            _pstCurrModify = _pstOuterModify;

                            deleteCount++;
                        }
                    }

                    //表示有新数据标明删除
                    if (deleteCount != 0)
                    {
                        incHitCount();

                        return TC_Multi_HashMap_Malloc::RT_OK;
                    }
                    else//表示所有数据都已经标明是删除的了
                        return TC_Multi_HashMap_Malloc::RT_DATA_DEL;

                }
            } while (mainKey.next());
        }

        return TC_Multi_HashMap_Malloc::RT_NO_DATA;
    }

    int TC_Multi_HashMap_Malloc::addZSet(const string &mk, const string &v, const uint32_t unHash, double iScore, uint32_t iExpireTime, uint8_t iVersion, bool bDirty, bool bInc, DELETETYPE deleteType, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        lock_iterator it = findZSet(mk, v, unHash%_hash.size(), ret);
        bool bNewBlock = false;

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        uint32_t iMainKeyAddr, iBlockAddr;

        bool bNewHead = false;

        incHitCount();

        uint8_t iBlockLevel = 0;
        uint32_t iAllocSize = 0;

        if (it == end())
        {
            // 数据尚不存在

            // 查找是否已经存在于主key链
            uint32_t mIndex = mhashIndex(mk);		// 主key索引
            iMainKeyAddr = find(mk, mIndex, ret);
            if (ret != RT_OK && ret != RT_ONLY_KEY)
            {
                return ret;
            }
            if (iMainKeyAddr == 0)
            {
                // 主key头尚不存在，新建一个
                uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length() + sizeof(Block::NodeLevel)*_pHead->_iMaxLevel + sizeof(uint8_t));
                iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(mIndex, iAllocSize, vtData);
                if (iMainKeyAddr == 0)
                {
                    return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
                }
                // 将主key设置到主key头中
                MainKey mainKey(this, iMainKeyAddr);

                string str(sizeof(Block::NodeLevel)*_pHead->_iMaxLevel + sizeof(uint8_t), 0);
                str[0] = _pHead->_iMaxLevel;
                str += mk;
                ret = mainKey.set(str.c_str(), str.length(), vtData);
                if (ret != TC_Multi_HashMap_Malloc::RT_OK)
                {
                    doRecover();
                    return ret;
                }

                bNewHead = true;
            }

            iBlockLevel = RandomHeight();
            iAllocSize = (uint32_t)(sizeof(Block::tagBlockHead) + v.length() + sizeof(Block::NodeLevel)*iBlockLevel + sizeof(uint8_t) + sizeof(uint64_t) * 2);

            // 先分配Blcok空间, 并获得淘汰的数据
            iBlockAddr = _pDataAllocator->allocateMemBlock(MainKey::ZSET_TYPE, iMainKeyAddr, unHash%_hash.size(), true, iAllocSize, vtData);
            if (iBlockAddr == 0)
            {
                doRecover();
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }

            it = HashMapLockIterator(this, iBlockAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);
            bNewBlock = true;
        }
        else
        {
            if (deleteType == DELETE_AUTO)
            {
                return RT_OK;
            }
            Block block(this, it->getAddr());
            if (deleteType != DELETE_AUTO)
            {
                if (block.isDelete() && (deleteType == DELETE_FALSE))
                {
                    MainKey mainKey(this, block.getBlockHead()->_iMainKey);
                    insertSkipListSpan(mainKey, block.getHead());
                }
                block.setDelete(deleteType == DELETE_TRUE);
            }

            //数据存在，更新score
            bool bChangePos = false;

            if (iVersion != 0 && block.getBlockHead()->_iVersion != iVersion)
            {
                // 数据版本不匹配
                return TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH;
            }

            if (iExpireTime != 0)
                block.setExpireTime(iExpireTime);

            double currScore = block.getScore();

            if (bInc)
            {
                iScore += currScore;
            }

            if (iScore == currScore)
            {
                block.setDirty(bDirty);
                block.refreshSetList();

                return TC_Multi_HashMap_Malloc::RT_OK;
            }
            else if (iScore > currScore)
            {
                uint32_t nextAddr = block.getNext(0);
                if (nextAddr != 0)
                {
                    Block NextBlock(this, nextAddr);
                    if (iScore > NextBlock.getScore())
                        bChangePos = true;
                    else
                        block.setScore(iScore);
                }
                else
                    block.setScore(iScore);
            }
            else
            {
                uint32_t preAddr = block.getBlockHead()->_iMKBlockPrev;
                if (preAddr != 0)
                {
                    Block preBlock(this, preAddr);
                    if (iScore < preBlock.getScore())
                        bChangePos = true;
                    else
                        block.setScore(iScore);
                }
                else
                    block.setScore(iScore);
            }

            if (bChangePos)
            {
                MainKey mainKey(this, block.getBlockHead()->_iMainKey);
                delSkipList(mainKey, it->getAddr());
                block.setScore(iScore);
                insertSkipList(mainKey, it->getAddr(), iScore);
            }

            block.setDirty(bDirty);

            block.refreshSetList();

            return TC_Multi_HashMap_Malloc::RT_OK;
        }

        string tmpData;
        tmpData.reserve(iAllocSize);
        tmpData += string((char*)&iBlockLevel, 1);
        tmpData += string(sizeof(Block::NodeLevel)*iBlockLevel, 0);
        tmpData += string((char*)&iScore, sizeof(double));

        tmpData += v;

        ret = it->set(mk, tmpData, iExpireTime, iVersion, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            // set数据失败，可能的情况是空间不够了
            if (bNewBlock)
            {
                // 如果是新set的数据，此时要注意删除前面分配并挂接的block头
                // 否则这个block将永远无法访问，也不能删除，因为block里是一个空的block
                //Block block(this, it->getAddr());
                //block.erase();
                // modified by smitchzhao @ 2011-10-02
                doRecover();
            }
            return ret;
        }

        if (bNewHead)
        {
            // 减主key的only key计数
            delOnlyKeyCountM();
        }

        MainKey mainKey(this, iMainKeyAddr);
        insertSkipList(mainKey, iBlockAddr, iScore);

        Block block(this, iBlockAddr);
        if (bNewBlock)
        {
            block.setSyncTime(time(NULL));
        }
        block.setDirty(bDirty);

        if (deleteType != DELETE_AUTO)
        {
            if (deleteType == DELETE_TRUE)
                delSkipListSpan(mainKey, block.getHead());
            block.setDelete(deleteType == DELETE_TRUE);
        }

        /*if(eType != AUTO_DATA)
        {
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            mainKey.SETFULLDATA(eType == FULL_DATA);
        }*/

        block.refreshSetList();

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::addZSet(const string &mk, const vector<Value> &vt, DATATYPE eType, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        for (unsigned int i = 0; i < vt.size(); i++)
        {
            uint32_t unHash = getHashFunctor()(mk + vt[i]._value);
            int iRet = addZSet(mk, vt[i]._value, unHash, vt[i]._score, vt[i]._iExpireTime, vt[i]._iVersion, vt[i]._dirty, false, vt[i]._isDelete, vtData);
            if (iRet != RT_OK)
                return iRet;
        }

        if (eType != AUTO_DATA)
        {
            // 虽然不需要更新value，但要更新数据状态
            setFullData(mk, eType == FULL_DATA);
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::addZSet(const string &mk, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        // 查找是否已经存在于主key链
        int ret;
        uint32_t mIndex = mhashIndex(mk);		// 主key索引
        uint32_t iMainKeyAddr = find(mk, mIndex, ret);
        if (ret != RT_OK && ret != RT_ONLY_KEY)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key头尚不存在，新建一个
            uint32_t iAllocSize = (uint32_t)(sizeof(MainKey::tagMainKeyHead) + mk.length() + sizeof(Block::NodeLevel)*_pHead->_iMaxLevel + sizeof(uint8_t));
            iMainKeyAddr = _pMainKeyAllocator->allocateMainKeyHead(mIndex, iAllocSize, vtData);
            if (iMainKeyAddr == 0)
            {
                return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
            }
            // 将主key设置到主key头中
            MainKey mainKey(this, iMainKeyAddr);

            string str(sizeof(Block::NodeLevel)*_pHead->_iMaxLevel + sizeof(uint8_t), 0);
            str[0] = _pHead->_iMaxLevel;
            str += mk;
            ret = mainKey.set(str.c_str(), str.length(), vtData);
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                doRecover();
                return ret;
            }
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::getZSet(const string &mk, const uint64_t iStart, const uint64_t iEnd, const bool bUp, const uint32_t iNowTime, list<Value> &vtData)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        incHitCount();

        MainKey mainKey(this, iMainKeyAddr);
        if (mainKey.getNext(0) == 0)
        {
            return RT_ONLY_KEY;
        }
        uint32_t iAddr = mainKey.getNext(0);

        uint64_t iIndex = 0;

        while (iAddr != 0 && iIndex <= iEnd)
        {
            Block block(this, iAddr);
            if (block.isDelete() == false && (iNowTime == 0 || block.getBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getBlockHead()->_iExpireTime > iNowTime)))
            {
                if (iIndex++ >= iStart)
                {
                    BlockData data;
                    ret = block.getZSetBlockData(data);
                    if (ret != RT_OK && ret != RT_ONLY_KEY)
                    {
                        return ret;
                    }
                    if (ret == RT_OK)
                    {
                        // 仅取非only key的数据
                        Value v(mk, data);
                        v._keyType = MainKey::ZSET_TYPE;

                        if (bUp)
                            vtData.push_back(v);
                        else
                            vtData.push_front(v);
                    }
                }
            }

            iAddr = block.getNext(0);
        }

        if (!mainKey.ISFULLDATA())
            return RT_PART_DATA;

        //修复老版本 get完以后没有刷新get链
        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::getZSet(const string &mk, const string &v, const uint32_t unHash, const uint32_t iNowTime, Value& vData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        int ret;
        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        MainKey mainKey(this, iMainKeyAddr);
        if (mainKey.getNext(0) == 0)
        {
            return RT_NO_DATA;
        }

        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        ret = TC_Multi_HashMap_Malloc::RT_OK;
        lock_iterator it = findZSet(mk, v, unHash%_hash.size(), ret);

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        incHitCount();

        if (it == end())
        {
            // 数据尚不存在
            return RT_NO_DATA;
        }
        else
        {
            //数据存在
            Block block(this, it->getAddr());
            if (block.isDelete() == false && (iNowTime == 0 || block.getBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getBlockHead()->_iExpireTime > iNowTime)))
            {
                BlockData data;
                ret = block.getZSetBlockData(data);
                if (ret == RT_OK)
                {
                    Value v(mk, data);
                    v._keyType = MainKey::ZSET_TYPE;

                    vData = v;
                }
                return ret;
            }
            else
                return RT_NO_DATA;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    // Changed by tutuli 2017-7-24 16:03
    // 支持getZSet的limit操作
    int TC_Multi_HashMap_Malloc::getZSetLimit(const string &mk, const size_t iStart, const size_t iDataCount, const bool bUp, const uint32_t iNowTime, list<Value> &vtData)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        incHitCount();

        MainKey mainKey(this, iMainKeyAddr);
        if (mainKey.getNext(0) == 0)
        {
            return RT_ONLY_KEY;
        }
        uint32_t iAddr = mainKey.getNext(0);
        // Changed by tutuli 2017-07-24 
        uint64_t iRealDataCount = 0;
        uint64_t iIndex = 0;

        while (iAddr != 0 && iRealDataCount < iDataCount)
        {
            Block block(this, iAddr);
            if (block.isDelete() == false && (iNowTime == 0 || block.getBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getBlockHead()->_iExpireTime > iNowTime)))
            {
                if (iIndex++ >= iStart)
                {
                    BlockData data;
                    ret = block.getZSetBlockData(data);
                    if (ret != RT_OK && ret != RT_ONLY_KEY)
                    {
                        return ret;
                    }
                    if (ret == RT_OK)
                    {
                        // 仅取非only key的数据
                        Value v(mk, data);
                        v._keyType = MainKey::ZSET_TYPE;

                        if (bUp)
                            vtData.push_back(v);
                        else
                            vtData.push_front(v);
                        iRealDataCount++;
                    }
                }
            }

            iAddr = block.getNext(0);
        }

        if (!mainKey.ISFULLDATA())
            return RT_PART_DATA;

        //修复老版本 get完以后没有刷新get链
        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        return RT_OK;
    }


    int TC_Multi_HashMap_Malloc::getScoreZSet(const string &mk, const string &v, const uint32_t unHash, const uint32_t iNowTime, double &iScore)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        int ret;
        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        MainKey mainKey(this, iMainKeyAddr);
        if (mainKey.getNext(0) == 0)
        {
            return RT_NO_DATA;
        }

        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        ret = TC_Multi_HashMap_Malloc::RT_OK;
        lock_iterator it = findZSet(mk, v, unHash%_hash.size(), ret);

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        incHitCount();

        if (it == end())
        {
            // 数据尚不存在
            return RT_NO_DATA;
        }
        else
        {
            //数据存在
            Block block(this, it->getAddr());
            if (block.isDelete() == false && (iNowTime == 0 || block.getBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getBlockHead()->_iExpireTime > iNowTime)))
            {
                iScore = block.getScore();

                return TC_Multi_HashMap_Malloc::RT_OK;
            }
            else
                return RT_NO_DATA;
        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::getRankZSet(const string &mk, const string &v, const uint32_t unHash, const bool order, const uint32_t iNowTime, long &iPos)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        MainKey mainKey(this, iMainKeyAddr);
        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        incHitCount();

        if (mainKey.getNext(0) == 0)
        {
            return RT_ONLY_KEY;
        }

        lock_iterator it = findZSet(mk, v, unHash%_hash.size(), ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        if (it == end())
        {
            // 数据尚不存在
            return RT_NO_DATA;
        }
        else
        {
            //数据存在
            Block block(this, it->getAddr());
            if (block.isDelete() == false && (iNowTime == 0 || block.getBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getBlockHead()->_iExpireTime > iNowTime)))
            {
                double iScore = block.getScore();

                iPos = 0;

                int level = mainKey.getLevel() - 1;
                Node *curr = &mainKey;
                while (level >= 0)
                {
                    if (0 == curr->getNext(level))
                    {
                        level--;
                        continue;
                    }

                    Block nextBlock(this, curr->getNext(level));
                    if (nextBlock.getScore() < iScore)
                    {
                        iPos += curr->getSpan(level);
                        block = nextBlock;
                        curr = &block;
                        continue;
                    }
                    else if (nextBlock.getScore() > iScore)
                    {
                        level--;
                        continue;
                    }
                    else if (curr->getNext(level) == it->getAddr())
                    {
                        iPos += curr->getSpan(level);
                        iPos--;
                        if (!order)
                        {
                            iPos = mainKey.getHeadPtr()->_iBlockCount - iPos - 1;
                        }
                        return TC_Multi_HashMap_Malloc::RT_OK;
                    }
                    else if (level != 0)
                    {
                        level--;
                        continue;
                    }
                    else
                    {//到最后一层，就要顺序找
                        while (0 != curr->getNext(level))
                        {
                            if (curr->getNext(level) != it->getAddr())
                            {
                                iPos += curr->getSpan(level);
                                Block nextBlock(this, curr->getNext(level));
                                block = nextBlock;
                                curr = &block;

                                if (nextBlock.getScore() != iScore)
                                    break;

                                continue;
                            }
                            else
                            {
                                iPos += curr->getSpan(level);
                                iPos--;
                                if (!order)
                                {
                                    iPos = mainKey.getHeadPtr()->_iBlockCount - iPos - 1;
                                }
                                return TC_Multi_HashMap_Malloc::RT_OK;
                            }
                        }

                        //到这里就是没找到，肯定有问题
                        return RT_EXCEPTION_ERR;
                    }
                }

                return RT_EXCEPTION_ERR;
            }
            else
                return RT_NO_DATA;
        }

        return RT_EXCEPTION_ERR;
    }

    int TC_Multi_HashMap_Malloc::getZSetByScore(const string &mk, const double iMin, const double iMax, const uint32_t iNowTime, list<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        incHitCount();

        MainKey mainKey(this, iMainKeyAddr);
        if (mainKey.getNext(0) == 0)
        {
            return RT_ONLY_KEY;
        }

        //先定位第一个数据
        int ilevel = mainKey.getLevel() - 1;
        uint32_t iAddr = 0;
        for (; ilevel > 0; ilevel--)
        {
            iAddr = mainKey.getNext(ilevel);
            if (iAddr == 0)
                continue;
            else
            {
                Block block(this, iAddr);
                double score = block.getScore();
                if (score < iMin)
                    break;
                else
                    continue;
            }
        }

        iAddr = mainKey.getNext(ilevel);

        for (; iAddr != 0 && ilevel > 0; ilevel--)
        {
            Block block(this, iAddr);
            double score = block.getScore();
            uint32_t nextAddr = block.getNext(ilevel);

            while (nextAddr != 0)
            {
                Block nextBlock(this, nextAddr);
                score = nextBlock.getScore();
                if (score < iMin)
                {
                    iAddr = nextAddr;
                    nextAddr = nextBlock.getNext(ilevel);
                }
                else
                    break;
            }
        }


        while (iAddr != 0)
        {
            Block block(this, iAddr);
            if (block.isDelete() == false && (iNowTime == 0 || block.getBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getBlockHead()->_iExpireTime > iNowTime)))
            {
                double score = block.getScore();
                if (iMin <= score && score <= iMax)
                {
                    if (block.isDelete() == false)
                    {
                        BlockData data;
                        ret = block.getZSetBlockData(data);
                        if (ret != RT_OK && ret != RT_ONLY_KEY)
                        {
                            return ret;
                        }
                        if (ret == RT_OK)
                        {
                            // 仅取非only key的数据
                            Value v(mk, data);
                            v._keyType = MainKey::ZSET_TYPE;

                            vtData.push_back(v);
                        }
                    }
                }
            }

            iAddr = block.getNext(0);
        }

        if (!mainKey.ISFULLDATA())
            return RT_PART_DATA;

        if (!_pHead->_bReadOnly)
        {
            mainKey.refreshGetList();
        }

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::delZSetSetBit(const string &mk, const string &v, const uint32_t unHash, const time_t t)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int    ret = RT_OK;
        Value tmpData;
        lock_iterator it = findZSet(mk, v, unHash%_hash.size(), ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        Block block(this, it->getAddr());
        MainKey mainKey(this, block.getBlockHead()->_iMainKey);

        //如果是标记为删除。就直接返回
        if (block.isDelete())
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;

        incHitCount();

        //设置删除位
        block.setDelete(true);

        //复用同步时间字段，用于记录删除的时间
        block.getBlockHead()->_iSyncTime = t;

        //重置版本号
        block.getBlockHead()->_iVersion = 1;

        block.setDirty(false);

        block.setExpireTime(0);

        delSkipListSpan(mainKey, it->getAddr());

        return ret;
    }

    int TC_Multi_HashMap_Malloc::delRangeZSetSetBit(const string &mk, const double iMin, const double iMax, const uint32_t iNowTime, const time_t t)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t index = mhashIndex(mk);
        uint32_t iMainKeyAddr = find(mk, index, ret);
        if (ret != RT_OK)
        {
            return ret;
        }
        if (iMainKeyAddr == 0)
        {
            // 主key不存在
            return RT_NO_DATA;
        }

        incHitCount();

        MainKey mainKey(this, iMainKeyAddr);
        if (mainKey.getNext(0) == 0)
        {
            return RT_ONLY_KEY;
        }
        uint32_t iAddr = mainKey.getNext(0);

        while (iAddr != 0)
        {
            Block block(this, iAddr);
            uint32_t nextAddr = block.getNext(0);
            if (iNowTime == 0 || block.getBlockHead()->_iExpireTime == 0 || (iNowTime != 0 && block.getBlockHead()->_iExpireTime > iNowTime))
            {
                double score = block.getScore();
                if (iMin <= score && score <= iMax)
                {
                    if (block.isDelete() == false)
                    {
                        _pstCurrModify = _pstInnerModify;
                        //设置删除位
                        block.setDelete(true);

                        //复用同步时间字段，用于记录删除的时间
                        block.getBlockHead()->_iSyncTime = t;

                        //重置版本号
                        block.getBlockHead()->_iVersion = 1;

                        block.setDirty(false);

                        block.setExpireTime(0);

                        delSkipListSpan(mainKey, block.getHead());

                        //这一条设置完毕，可以清理保护区
                        doUpdate();

                        _pstCurrModify = _pstOuterModify;
                    }
                }
            }

            iAddr = nextAddr;
        }

        return RT_OK;
    }

    int TC_Multi_HashMap_Malloc::delZSetReal(const string &mk, const string &v, const uint32_t unHash)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        if (_pHead->_bReadOnly) return RT_READONLY;

        int    ret = RT_OK;
        uint32_t index = hashIndex(mk, v);
        Value tmpData;
        lock_iterator it = findZSet(mk, v, index, ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }

        if (it == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        Block block(this, it->getAddr());

        //如果是标记为删除。就可以删除
        if (block.isDelete())
        {
            // 把主key下数据个数增加一个，因为后面的block.erase()会把这个计数减一
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);
            saveValue(&mainKey.getHeadPtr()->_iBlockCount, mainKey.getHeadPtr()->_iBlockCount + 1);

            //delSkipList(mainKey, it->getAddr());
            block.eraseZSet(false);

            //如果mainKey下没数据了，就把mainKey也删除
            if (mainKey.getNext(0) == 0)
            {
                vector<Value> vtData;
                mainKey.erase(vtData);
            }
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::delZSetSetBit(const string &mk, const time_t t)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        if (_pHead->_bReadOnly) return RT_READONLY;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        uint32_t iIndex = mhashIndex(mk);
        if (itemMainKey(iIndex)->_iMainKeyAddr != 0)
        {
            MainKey mainKey(this, itemMainKey(iIndex)->_iMainKeyAddr);
            do
            {
                string sKey;
                mainKey.getData(sKey);
                if (mk == sKey)
                {
                    // 找到主key头了
                    incHitCount();

                    int deleteCount = 0;
                    uint32_t iAddr = mainKey.getNext(0);
                    if (iAddr == 0)
                    {
                        vector<Value> vtData;
                        return mainKey.erase(vtData);
                    }
                    while (iAddr != 0)
                    {
                        Block block(this, iAddr);
                        iAddr = block.getNext(0);

                        if (block.isDelete() == false)
                        {
                            _pstCurrModify = _pstInnerModify;
                            //设置删除标志
                            block.setDelete(true);

                            //复用同步时间，用于记录删除时间
                            saveValue(&(block.getBlockHead()->_iSyncTime), (uint32_t)t);

                            //重置版本号
                            saveValue(&(block.getBlockHead()->_iVersion), (uint8_t)1);

                            block.setDirty(false);

                            block.setExpireTime(0);

                            delSkipListSpan(mainKey, block.getHead());

                            //这一条设置完毕，可以清理保护区
                            doUpdate();

                            _pstCurrModify = _pstOuterModify;

                            deleteCount++;
                        }
                    }

                    //表示有新数据标明删除
                    if (deleteCount != 0)
                    {
                        incHitCount();

                        return TC_Multi_HashMap_Malloc::RT_OK;
                    }
                    else//表示所有数据都已经标明是删除的了
                        return TC_Multi_HashMap_Malloc::RT_DATA_DEL;

                }
            } while (mainKey.next());
        }

        return TC_Multi_HashMap_Malloc::RT_NO_DATA;
    }

    int TC_Multi_HashMap_Malloc::updateZSet(const string &mk, const string &oldValue, const string &newValue, double iScore, uint32_t iExpireTime, char iVersion, bool bDirty, bool bOnlyScore, vector<Value> &vtData)
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::ZSET_TYPE != keyType)
            return TC_Multi_HashMap_Malloc::RT_EXCEPTION_ERR;

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        if (_pHead->_bReadOnly) return RT_READONLY;

        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        uint32_t unHashOld = getHashFunctor()(mk + oldValue);
        lock_iterator itOld = findZSet(mk, oldValue, unHashOld%_hash.size(), ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK && ret != TC_Multi_HashMap_Malloc::RT_ONLY_KEY)
        {
            return ret;
        }
        if (itOld == end())
        {
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        uint32_t unHashNew = getHashFunctor()(mk + newValue);

        lock_iterator it = findZSet(mk, newValue, unHashNew%_hash.size(), ret);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        if (it != end())
        {
            Block block(this, it->getAddr());
            MainKey mainKey(this, block.getBlockHead()->_iMainKey);

            //若只修改score或新数据被删除，则更新原数据，其他则报错
            if (bOnlyScore || block.isDelete())
            {
                //新数据已删除，则重新加入主key链中，并设置删除标志位false
                if (block.isDelete())
                {
                    block.setDelete(false);
                    insertSkipListSpan(mainKey, block.getHead());
                }

                //更新score
                bool bChangePos = false;

                if (iVersion != 0 && block.getBlockHead()->_iVersion != iVersion)
                {
                    // 数据版本不匹配
                    return TC_Multi_HashMap_Malloc::RT_DATA_VER_MISMATCH;
                }

                if (iExpireTime != 0)
                    block.setExpireTime(iExpireTime);

                double currScore = block.getScore();
                if (iScore == currScore)
                {
                    block.setDirty(bDirty);
                    block.refreshSetList();
                    return TC_Multi_HashMap_Malloc::RT_OK;
                }
                else if (iScore > currScore)
                {
                    uint32_t nextAddr = block.getNext(0);
                    if (nextAddr != 0)
                    {
                        Block NextBlock(this, nextAddr);
                        if (iScore > NextBlock.getScore())
                            bChangePos = true;
                        else
                            block.setScore(iScore);
                    }
                    else
                    {
                        block.setScore(iScore);
                    }
                }
                else
                {
                    uint32_t preAddr = block.getBlockHead()->_iMKBlockPrev;
                    if (preAddr != 0)
                    {
                        Block preBlock(this, preAddr);
                        if (iScore < preBlock.getScore())
                            bChangePos = true;
                        else
                            block.setScore(iScore);
                    }
                    else
                        block.setScore(iScore);
                }

                if (bChangePos)
                {
                    MainKey mainKey(this, block.getBlockHead()->_iMainKey);
                    delSkipList(mainKey, it->getAddr());
                    block.setScore(iScore);
                    insertSkipList(mainKey, it->getAddr(), iScore);
                }
                block.setDirty(bDirty);
                block.refreshSetList();

                return TC_Multi_HashMap_Malloc::RT_OK;
            }
            else
            {
                return TC_Multi_HashMap_Malloc::RT_DATA_EXIST;
            }
        }

        Block block(this, itOld->getAddr());
        MainKey mainKey(this, block.getBlockHead()->_iMainKey);

        //删除老数据
        if (block.isDelete())
        {
            return TC_Multi_HashMap_Malloc::RT_DATA_DEL;;
        }
        incHitCount();

        //设置删除位
        block.setDelete(true);

        //复用同步时间字段，用于记录删除的时间
        block.getBlockHead()->_iSyncTime = time(NULL);

        //重置版本号
        block.getBlockHead()->_iVersion = 1;
        block.setDirty(false);
        block.setExpireTime(0);

        delSkipListSpan(mainKey, itOld->getAddr());

        //重新申请内存保存新数据
        uint8_t iBlockLevel = RandomHeight();
        uint32_t iAllocSize = (uint32_t)(sizeof(Block::tagBlockHead) + newValue.length() + sizeof(Block::NodeLevel)*iBlockLevel + sizeof(uint8_t) + sizeof(uint64_t) * 2);

        uint32_t iBlockAddr = _pDataAllocator->allocateMemBlock(MainKey::ZSET_TYPE, mainKey.getHead(), unHashNew%_hash.size(), true, iAllocSize, vtData);
        if (iBlockAddr == 0)
        {
            doRecover();
            return TC_Multi_HashMap_Malloc::RT_NO_MEMORY;
        }

        it = HashMapLockIterator(this, iBlockAddr, HashMapLockIterator::IT_BLOCK, HashMapLockIterator::IT_NEXT);

        string tmpData;
        tmpData.reserve(iAllocSize);
        tmpData += string((char*)&iBlockLevel, 1);
        tmpData += string(sizeof(Block::NodeLevel)*iBlockLevel, 0);
        tmpData += string((char*)&iScore, sizeof(double));

        tmpData += newValue;

        ret = it->set(mk, tmpData, iExpireTime, iVersion, vtData);
        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            doRecover();
            return ret;
        }

        insertSkipList(mainKey, iBlockAddr, iScore);

        Block newBlock(this, iBlockAddr);

        //过期时间
        uint32_t _iExpireTime = 0;
        if (iExpireTime != 0)
        {
            _iExpireTime = iExpireTime;
        }
        else
        {
            _iExpireTime = block.getExpireTime();
        }
        newBlock.setExpireTime(_iExpireTime);

        newBlock.setSyncTime(time(NULL));
        newBlock.setDirty(bDirty);

        newBlock.refreshSetList();

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    void TC_Multi_HashMap_Malloc::sync()
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        _pHead->_iSyncTail = _pHead->_iDirtyTail;
    }

    int TC_Multi_HashMap_Malloc::sync(uint32_t iNowTime, Value &data)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        uint32_t iAddr = _pHead->_iSyncTail;

        //到链表头部了, 返回RT_OK
        if (iAddr == 0)
        {
            return RT_OK;
        }

        Block block(this, iAddr);

        _pHead->_iSyncTail = block.getBlockHead()->_iSetPrev;

        //当前数据是干净数据,或者是已经标记为删除，就不需要同步了
        if ((!block.isDirty()) || (block.isDelete()))
        {
            if (_pHead->_iDirtyTail == iAddr)
            {
                _pHead->_iDirtyTail = block.getBlockHead()->_iSetPrev;
            }
            return RT_NONEED_SYNC;
        }

        // 取出主key
        MainKey mainKey(this, block.getBlockHead()->_iMainKey);
        MainKey::KEYTYPE keyKey = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (keyKey != MainKey::ZSET_TYPE)
            mainKey.get(data._mkey);
        else
            mainKey.getData(data._mkey);

        BlockData blkData;
        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        if (keyKey == MainKey::HASH_TYPE)
            ret = block.getBlockData(blkData);
        else if (keyKey == MainKey::LIST_TYPE)
            ret = block.getListBlockData(blkData);
        else if (keyKey == MainKey::SET_TYPE)
            ret = block.getSetBlockData(blkData);
        else if (keyKey == MainKey::ZSET_TYPE)
            ret = block.getZSetBlockData(blkData);

        data.assign(blkData);

        if (keyKey != MainKey::LIST_TYPE)
        {
            if (ret != TC_Multi_HashMap_Malloc::RT_OK)
            {
                //没有获取完整的记录
                if (_pHead->_iDirtyTail == iAddr)
                {
                    _pHead->_iDirtyTail = block.getBlockHead()->_iSetPrev;
                }
                return ret;
            }

            //脏数据且超过_pHead->_iSyncTime没有回写, 需要回写
            if (_pHead->_iSyncTime + data._iSyncTime < iNowTime && block.isDirty())
            {
                block.setDirty(false);
                block.setSyncTime(iNowTime);

                if (_pHead->_iDirtyTail == iAddr)
                {
                    _pHead->_iDirtyTail = block.getBlockHead()->_iSetPrev;
                }

                return RT_NEED_SYNC;
            }
        }

        //脏数据, 但是不需要回写, 脏链表不往前推
        return RT_NONEED_SYNC;
    }

    void TC_Multi_HashMap_Malloc::backup(bool bForceFromBegin)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        if (bForceFromBegin || _pHead->_iBackupTail == 0)
        {
            //移动备份指针到Get链尾部, 准备开始备份数据
            _pHead->_iBackupTail = _pHead->_iGetTail;
        }
    }

    int TC_Multi_HashMap_Malloc::backup(vector<Value> &vtData)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        uint32_t iAddr = _pHead->_iBackupTail;

        //到链表头部了, 返回RT_OK
        if (iAddr == 0)
        {
            return RT_OK;
        }

        // 取出主key
        MainKey mainKey(this, iAddr);
        MainKey::KEYTYPE keyKey = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        string mk;
        if (keyKey != MainKey::ZSET_TYPE)
            mainKey.get(mk);
        else
            mainKey.getData(mk);

        // 一次备份整个主key
        int ret = RT_OK;
        lock_iterator it(this, mainKey.getHeadPtr()->_iBlockHead, lock_iterator::IT_MKEY, lock_iterator::IT_NEXT);
        while (it != end())
        {
            Block block(this, it->_iAddr);
            if (block.isDelete() == false)
            {
                BlockData blkData;
                if (keyKey == MainKey::HASH_TYPE)
                    ret = block.getBlockData(blkData);
                else if (keyKey == MainKey::LIST_TYPE)
                    ret = block.getListBlockData(blkData);
                else if (keyKey == MainKey::SET_TYPE)
                    ret = block.getSetBlockData(blkData);
                else if (keyKey == MainKey::ZSET_TYPE)
                    ret = block.getZSetBlockData(blkData);
                if (ret != RT_OK)
                {
                    break;
                }
                Value value(mk, blkData);
                vtData.push_back(value);
            }

            it++;
        }

        //迁移一次
        _pHead->_iBackupTail = mainKey.getHeadPtr()->_iGetPrev;

        if (ret == RT_OK)
        {
            if (mainKey.ISFULLDATA())
            {
                return RT_NEED_BACKUP;
            }
            else
            {
                return RT_PART_DATA;
            }
        }

        return ret;
    }

    int TC_Multi_HashMap_Malloc::calculateData(uint32_t &count, bool &isEnd)
    {
        /*MainKey::KEYTYPE keyKey = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (keyKey != MainKey::HASH_TYPE)
            return TC_Multi_HashMap_Malloc::RT_OK;*/

        /*
            //用于保存block的数据
            string tmp;

            count = 0;

            uint32_t iBlockAddr = mainKey.getHeadPtr()->_iBlockHead;
            while(iBlockAddr != 0)
            {
                Block block(this, iBlockAddr);
                iBlockAddr = block.getBlockHead()->_iMKBlockNext;

                if(block.isDelete() != true)
                {
                    tmp = "";
                    int ret = block.get(tmp);
                    if(ret == TC_Multi_HashMap_Malloc::RT_OK)
                    {
                        //数据大小+头部
                        count += tmp.size() + 50;
                    }
                    else
                        return ret;
                }
            }

            //加上主key头
            count += 41;
        */
        //只计算条数，每次可以多读一些
        int num = 3000;

        count = 0;
        isEnd = false;

        while (num-- > 0)
        {
            if (0 == _iReadP)
            {
                isEnd = true;
                _iReadP = _iReadPBak;
                return TC_Multi_HashMap_Malloc::RT_OK;
            }

            // 取出主key
            MainKey mainKey(this, _iReadP);

            //只计算主key下的记录条数
            count += mainKey.getHeadPtr()->_iBlockCount;
            _iReadP = mainKey.getHeadPtr()->_iGetNext;

        }

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    int TC_Multi_HashMap_Malloc::resetCalculatePoint()
    {
        _iReadP = _pHead->_iGetHead;
        _iReadPBak = _pHead->_iGetHead;

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::begin()
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        for (size_t i = 0; i < _hash.size(); i++)
        {
            tagHashItem &hashItem = _hash[i];
            if (hashItem._iBlockAddr != 0)
            {
                return lock_iterator(this, hashItem._iBlockAddr, lock_iterator::IT_BLOCK, lock_iterator::IT_NEXT);
            }
        }

        return end();
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::rbegin()
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        for (size_t i = _hash.size(); i > 0; i--)
        {
            tagHashItem &hashItem = _hash[i - 1];
            if (hashItem._iBlockAddr != 0)
            {
                Block block(this, hashItem._iBlockAddr);
                return lock_iterator(this, block.getLastBlockHead(), lock_iterator::IT_BLOCK, lock_iterator::IT_PREV);
            }
        }

        return end();
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::beginSetTime()
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        return lock_iterator(this, _pHead->_iSetHead, lock_iterator::IT_SET, lock_iterator::IT_NEXT);
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::rbeginSetTime()
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        return lock_iterator(this, _pHead->_iSetTail, lock_iterator::IT_SET, lock_iterator::IT_PREV);
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::beginGetTime()
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        if (_pHead->_iGetHead != 0)
        {
            MainKey mainKey(this, _pHead->_iGetHead);
            return lock_iterator(this, mainKey.getHeadPtr()->_iBlockHead, lock_iterator::IT_GET, lock_iterator::IT_NEXT);
        }
        else
        {
            return lock_iterator(this, 0, lock_iterator::IT_GET, lock_iterator::IT_NEXT);
        }
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::rbeginGetTime()
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        if (_pHead->_iGetTail != 0)
        {
            MainKey mainKey(this, _pHead->_iGetTail);
            // 找到主key链上的最后一个数据
            uint32_t iAddr = mainKey.getHeadPtr()->_iBlockHead;
            if (iAddr)
            {
                Block block(this, iAddr);
                while (block.getBlockHead()->_iMKBlockNext)
                {
                    iAddr = block.getBlockHead()->_iMKBlockNext;
                    block = Block(this, iAddr);
                }
            }
            return lock_iterator(this, iAddr, lock_iterator::IT_GET, lock_iterator::IT_PREV);
        }
        else
        {
            return lock_iterator(this, 0, lock_iterator::IT_GET, lock_iterator::IT_PREV);
        }
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::beginDirty()
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        return lock_iterator(this, _pHead->_iDirtyTail, lock_iterator::IT_SET, lock_iterator::IT_PREV);
    }

    TC_Multi_HashMap_Malloc::hash_iterator TC_Multi_HashMap_Malloc::hashBegin()
    {
        // 注意，对于hash_iterator一定不能像上面一样使用自动恢复，因为它是不加锁的
        //TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        // 还是需要自动恢复，但要保证在外层加锁
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        return hash_iterator(this, 0);
    }
    TC_Multi_HashMap_Malloc::mhash_iterator TC_Multi_HashMap_Malloc::mHashBegin()
    {
        // 注意，对于hash_iterator一定不能像上面一样使用自动恢复，因为它是不加锁的
        //TC_Multi_HashMap::FailureRecover recover(this);
        // 还是需要自动恢复，但要保证在外层加锁
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);
        return mhash_iterator(this, 0);
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    string TC_Multi_HashMap_Malloc::desc()
    {
        ostringstream s;
        {
            s << "[Version                 = " << (int)_pHead->_cMaxVersion << "." << (int)_pHead->_cMinVersion << "]" << endl;
            s << "[ReadOnly                = " << _pHead->_bReadOnly << "]" << endl;
            s << "[AutoErase               = " << _pHead->_bAutoErase << "]" << endl;
            s << "[MemSize                 = " << _pHead->_iMemSize << "]" << endl;
            if (_iMainKeySize > 0)
            {
                s << "[MainKeyCapacity         = " << _pMainKeyAllocator->getAllCapacity() << "]" << endl;
                s << "[UsedMainKeyMem          = " << _pHead->_iMKUsedMem << "]" << endl;
                s << "[FreeMainKeyMem          = " << _pMainKeyAllocator->getAllCapacity() - _pHead->_iMKUsedMem << "]" << endl;
            }
            s << "[MainKeyUsedChunk        = " << _pHead->_iMKUsedChunk << "]" << endl;
            s << "[MainKeyUsedMem          = " << _pHead->_iMKUsedMem << "]" << endl;
            s << "[DataCapacity            = " << _pDataAllocator->getAllCapacity() << "]" << endl;
            s << "[SingleBlockDataCapacity = " << TC_Common::tostr(_pDataAllocator->getSingleBlockCapacity()) << "]" << endl;
            s << "[DataUsedChunk    = " << _pHead->_iDataUsedChunk << "]" << endl;
            s << "[UsedDataMem      = " << _pHead->_iUsedDataMem << "]" << endl;
            s << "[FreeDataMem      = " << _pDataAllocator->getAllCapacity() - _pHead->_iUsedDataMem << "]" << endl;
            s << "[MainKeySize      = " << _pHead->_iMainKeySize << "]" << endl;
            s << "[DataSize         = " << _pHead->_iDataSize << "]" << endl;
            s << "[MainKeyHashCount = " << _hashMainKey.size() << "]" << endl;
            s << "[HashCount        = " << _hash.size() << "]" << endl;
            s << "[MainKeyRatio     = " << _pHead->_fMainKeyRatio << "]" << endl;
            s << "[HashRatio        = " << _pHead->_fHashRatio << "]" << endl;
            s << "[MainKeyCount     = " << _pHead->_iMainKeyCount << "]" << endl;
            s << "[ElementCount     = " << _pHead->_iElementCount << "]" << endl;
            s << "[SetHead          = " << _pHead->_iSetHead << "]" << endl;
            s << "[SetTail          = " << _pHead->_iSetTail << "]" << endl;
            s << "[GetHead          = " << _pHead->_iGetHead << "]" << endl;
            s << "[GetTail          = " << _pHead->_iGetTail << "]" << endl;
            s << "[DirtyTail        = " << _pHead->_iDirtyTail << "]" << endl;
            s << "[SyncTail         = " << _pHead->_iSyncTail << "]" << endl;
            s << "[SyncTime         = " << _pHead->_iSyncTime << "]" << endl;
            s << "[BackupTail       = " << _pHead->_iBackupTail << "]" << endl;
            s << "[DirtyCount       = " << _pHead->_iDirtyCount << "]" << endl;
            s << "[GetCount         = " << _pHead->_iGetCount << "]" << endl;
            s << "[HitCount         = " << _pHead->_iHitCount << "]" << endl;
            s << "[HitRatio         = " << (_pHead->_iGetCount == 0 ? 0 : _pHead->_iHitCount * 100 / _pHead->_iGetCount) << "]" << endl;
            s << "[OnlyKeyCount     = " << _pHead->_iOnlyKeyCount << "]" << endl;
            s << "[MKOnlyKeyCount   = " << _pHead->_iMKOnlyKeyCount << "]" << endl;
            s << "[MaxMKBlockCount  = " << _pHead->_iMaxBlockCount << "]" << endl;
            s << "[ModifyStatus     = " << (int)_pstOuterModify->_cModifyStatus << "]" << endl;
            s << "[ModifyIndex      = " << _pstOuterModify->_iNowIndex << "]" << endl;
        }

        //status管理命令过慢 去掉hash比例显示
        //uint32_t iMaxHash;
        //uint32_t iMinHash;
        //float fAvgHash;

        //analyseHash(iMaxHash, iMinHash, fAvgHash);
        //{
        //	s << "[MaxHash          = "   << iMaxHash      << "]" << endl;
        //	s << "[MinHash          = "   << iMinHash     << "]" << endl;
        //	s << "[AvgHash          = "   << fAvgHash     << "]" << endl;
        //}

        //analyseHashM(iMaxHash, iMinHash, fAvgHash);
        //{
        //	s << "[MaxMainkeyHash   = "   << iMaxHash      << "]" << endl;
        //	s << "[MinMainKeyHash   = "   << iMinHash     << "]" << endl;
        //	s << "[AvgMainKeyHash   = "   << fAvgHash     << "]" << endl;
        //}

        return s.str();
    }

    string TC_Multi_HashMap_Malloc::descWithHash()
    {
        ostringstream s;
        {
            s << "[Version                 = " << (int)_pHead->_cMaxVersion << "." << (int)_pHead->_cMinVersion << "]" << endl;
            s << "[ReadOnly                = " << _pHead->_bReadOnly << "]" << endl;
            s << "[AutoErase               = " << _pHead->_bAutoErase << "]" << endl;
            s << "[MemSize                 = " << _pHead->_iMemSize << "]" << endl;
            if (_iMainKeySize > 0)
            {
                s << "[MainKeyCapacity         = " << _pMainKeyAllocator->getAllCapacity() << "]" << endl;
                s << "[UsedMainKeyMem          = " << _pHead->_iMKUsedMem << "]" << endl;
                s << "[FreeMainKeyMem          = " << _pMainKeyAllocator->getAllCapacity() - _pHead->_iMKUsedMem << "]" << endl;
            }
            s << "[MainKeyUsedChunk        = " << _pHead->_iMKUsedChunk << "]" << endl;
            s << "[MainKeyUsedMem          = " << _pHead->_iMKUsedMem << "]" << endl;
            s << "[DataCapacity            = " << _pDataAllocator->getAllCapacity() << "]" << endl;
            s << "[SingleBlockDataCapacity = " << TC_Common::tostr(_pDataAllocator->getSingleBlockCapacity()) << "]" << endl;
            s << "[DataUsedChunk    = " << _pHead->_iDataUsedChunk << "]" << endl;
            s << "[UsedDataMem      = " << _pHead->_iUsedDataMem << "]" << endl;
            s << "[FreeDataMem      = " << _pDataAllocator->getAllCapacity() - _pHead->_iUsedDataMem << "]" << endl;
            s << "[MainKeySize      = " << _pHead->_iMainKeySize << "]" << endl;
            s << "[DataSize         = " << _pHead->_iDataSize << "]" << endl;
            s << "[MainKeyHashCount = " << _hashMainKey.size() << "]" << endl;
            s << "[HashCount        = " << _hash.size() << "]" << endl;
            s << "[MainKeyRatio     = " << _pHead->_fMainKeyRatio << "]" << endl;
            s << "[HashRatio        = " << _pHead->_fHashRatio << "]" << endl;
            s << "[MainKeyCount     = " << _pHead->_iMainKeyCount << "]" << endl;
            s << "[ElementCount     = " << _pHead->_iElementCount << "]" << endl;
            s << "[SetHead          = " << _pHead->_iSetHead << "]" << endl;
            s << "[SetTail          = " << _pHead->_iSetTail << "]" << endl;
            s << "[GetHead          = " << _pHead->_iGetHead << "]" << endl;
            s << "[GetTail          = " << _pHead->_iGetTail << "]" << endl;
            s << "[DirtyTail        = " << _pHead->_iDirtyTail << "]" << endl;
            s << "[SyncTail         = " << _pHead->_iSyncTail << "]" << endl;
            s << "[SyncTime         = " << _pHead->_iSyncTime << "]" << endl;
            s << "[BackupTail       = " << _pHead->_iBackupTail << "]" << endl;
            s << "[DirtyCount       = " << _pHead->_iDirtyCount << "]" << endl;
            s << "[GetCount         = " << _pHead->_iGetCount << "]" << endl;
            s << "[HitCount         = " << _pHead->_iHitCount << "]" << endl;
            s << "[HitRatio         = " << (_pHead->_iGetCount == 0 ? 0 : _pHead->_iHitCount * 100 / _pHead->_iGetCount) << "]" << endl;
            s << "[OnlyKeyCount     = " << _pHead->_iOnlyKeyCount << "]" << endl;
            s << "[MKOnlyKeyCount   = " << _pHead->_iMKOnlyKeyCount << "]" << endl;
            s << "[MaxMKBlockCount  = " << _pHead->_iMaxBlockCount << "]" << endl;
            s << "[ModifyStatus     = " << (int)_pstOuterModify->_cModifyStatus << "]" << endl;
            s << "[ModifyIndex      = " << _pstOuterModify->_iNowIndex << "]" << endl;
        }

        //status管理命令过慢 去掉hash比例显示
        uint32_t iMaxHash;
        uint32_t iMinHash;
        float fAvgHash;

        analyseHash(iMaxHash, iMinHash, fAvgHash);
        {
            s << "[MaxHash          = " << iMaxHash << "]" << endl;
            s << "[MinHash          = " << iMinHash << "]" << endl;
            s << "[AvgHash          = " << fAvgHash << "]" << endl;
        }

        analyseHashM(iMaxHash, iMinHash, fAvgHash);
        {
            s << "[MaxMainkeyHash   = " << iMaxHash << "]" << endl;
            s << "[MinMainKeyHash   = " << iMinHash << "]" << endl;
            s << "[AvgMainKeyHash   = " << fAvgHash << "]" << endl;
        }

        return s.str();
    }

    size_t TC_Multi_HashMap_Malloc::eraseExcept(uint32_t iNowAddr, vector<Value> &vtData)
    {
        //不能被淘汰
        if (!_pHead->_bAutoErase) return 0;

        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);
        if (MainKey::HASH_TYPE != keyType)
            return 0;

        size_t n = _pHead->_iEraseCount;
        if (n == 0) n = 10;
        size_t d = n;

        while (d != 0)
        {
            uint32_t iAddr;

            //判断按照哪种方式淘汰
            if (_pHead->_cEraseMode == TC_Multi_HashMap_Malloc::ERASEBYSET)
            {
                // 按set链淘汰
                iAddr = _pHead->_iSetTail;
                if (iAddr == 0)
                {
                    break;
                }
                // set链是block链，换算成主key头的地址
                Block block(this, iAddr);
                iAddr = block.getBlockHead()->_iMainKey;
            }
            else
            {
                // 按get链淘汰
                iAddr = _pHead->_iGetTail;
                if (iAddr == 0)
                {
                    break;
                }
            }

            // 不管是按get链淘汰还是按set链淘汰，都是要淘汰主key下所有数据
            MainKey mainKey(this, iAddr);
            if (iNowAddr == iAddr)
            {
                // 当前主key链中有正在分配的block，跳过
                iAddr = mainKey.getHeadPtr()->_iGetPrev;
            }
            if (iAddr == 0)
            {
                break;
            }

            uint32_t iBlockCount = mainKey.getHeadPtr()->_iBlockCount;
            if (d >= iBlockCount)
            {
                // 淘汰主key链下的所有数据
                int ret = mainKey.erase(vtData);
                if (ret == RT_OK)
                {
                    d -= iBlockCount;
                }
            }
            else
            {
                if (d == n)
                {
                    // 设置的要删除的数量太小，没有满足条件的主key，直接删除
                    int ret = mainKey.erase(vtData);
                    if (ret == RT_OK)
                    {
                        n = d + iBlockCount;	// 使返回正确的数据量
                    }
                }
                break;
            }
        }

        return n - d;
    }

    uint32_t TC_Multi_HashMap_Malloc::hashIndex(const string &mk, const string &uk)
    {
        // mk是主key，uk是除主key外的辅key，二者加起来作为联合主键
        return hashIndex(mk + uk);
    }

    uint32_t TC_Multi_HashMap_Malloc::hashIndex(const string& k)
    {
        return _hashf(k) % _hash.size();
    }

    uint32_t TC_Multi_HashMap_Malloc::mhashIndex(const string &mk)
    {
        if (_mhashf)
        {
            return _mhashf(mk) % _hashMainKey.size();
        }
        else
        {
            // 如果没有单独指定主key hash函数，则使用联合主键的hash函数
            return _hashf(mk) % _hashMainKey.size();
        }
    }

    uint32_t TC_Multi_HashMap_Malloc::mhash(const string &mk)
    {
        if (_mhashf)
        {
            return _mhashf(mk);
        }
        else
        {
            // 如果没有单独指定主key hash函数，则使用联合主键的hash函数
            return _hashf(mk);
        }
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::find(const string &mk, const string &uk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        uint32_t index = hashIndex(mk, uk);
        int ret = TC_Multi_HashMap_Malloc::RT_OK;

        if (item(index)->_iBlockAddr == 0)
        {
            return end();
        }

        Block mb(this, item(index)->_iBlockAddr);
        while (true)
        {
            HashMapLockItem mcmdi(this, mb.getHead());
            if (mcmdi.equal(mk, uk, ret))
            {
                // 找到了，不仅索引相同，key也相同
                incHitCount();
                return lock_iterator(this, mb.getHead(), lock_iterator::IT_BLOCK, lock_iterator::IT_NEXT);
            }

            if (!mb.nextBlock())
            {
                return end();
            }
        }

        return end();
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::find(const string &mk, const string &uk, uint32_t index, Value &v, int &ret)
    {
        ret = TC_Multi_HashMap_Malloc::RT_OK;

        if (item(index)->_iBlockAddr == 0)
        {
            return end();
        }

        Block mb(this, item(index)->_iBlockAddr);
        while (true)
        {
            HashMapLockItem mcmdi(this, mb.getHead());
            if (mcmdi.equal(mk, uk, v, ret))
            {
                // 找到了，不仅索引相同，key也相同
                return lock_iterator(this, mb.getHead(), lock_iterator::IT_BLOCK, lock_iterator::IT_NEXT);
            }

            if (!mb.nextBlock())
            {
                return end();
            }
        }

        return end();
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::find(const string &mk, const string &uk, uint32_t index, int &ret)
    {
        ret = TC_Multi_HashMap_Malloc::RT_OK;

        if (item(index)->_iBlockAddr == 0)
        {
            return end();
        }

        Block mb(this, item(index)->_iBlockAddr);
        while (true)
        {
            HashMapLockItem mcmdi(this, mb.getHead());
            if (mcmdi.equal(mk, uk, ret))
            {
                // 找到了，不仅索引相同，key也相同
                return lock_iterator(this, mb.getHead(), lock_iterator::IT_BLOCK, lock_iterator::IT_NEXT);
            }

            if (!mb.nextBlock())
            {
                return end();
            }
        }

        return end();
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::findSet(const string &mk, const string &v, uint32_t index, int &ret)
    {
        ret = TC_Multi_HashMap_Malloc::RT_OK;

        if (item(index)->_iBlockAddr == 0)
        {
            return end();
        }

        Block mb(this, item(index)->_iBlockAddr);
        while (true)
        {
            HashMapLockItem mcmdi(this, mb.getHead());
            if (mcmdi.equalSet(mk, v, ret))
            {
                // 找到了，不仅索引相同，key也相同
                return lock_iterator(this, mb.getHead(), lock_iterator::IT_BLOCK, lock_iterator::IT_NEXT);
            }

            if (!mb.nextBlock())
            {
                return end();
            }
        }

        return end();
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::findZSet(const string &mk, const string &v, uint32_t index, int &ret)
    {
        ret = TC_Multi_HashMap_Malloc::RT_OK;

        if (item(index)->_iBlockAddr == 0)
        {
            return end();
        }

        Block mb(this, item(index)->_iBlockAddr);
        while (true)
        {
            HashMapLockItem mcmdi(this, mb.getHead());
            if (mcmdi.equalZSet(mk, v, ret))
            {
                // 找到了，不仅索引相同，key也相同
                return lock_iterator(this, mb.getHead(), lock_iterator::IT_BLOCK, lock_iterator::IT_NEXT);
            }

            if (!mb.nextBlock())
            {
                return end();
            }
        }

        return end();
    }

    uint32_t TC_Multi_HashMap_Malloc::find(const string &mk, uint32_t index, int &ret)
    {
        ret = TC_Multi_HashMap_Malloc::RT_OK;

        if (itemMainKey(index)->_iMainKeyAddr == 0)
        {
            return 0;
        }

        MainKey mainKey(this, itemMainKey(index)->_iMainKeyAddr);
        do
        {
            string sKey;
            if (_pHead->_iKeyType == MainKey::ZSET_TYPE)
                mainKey.getData(sKey);
            else
                mainKey.get(sKey);

            if (ret != RT_OK)
            {
                return 0;
            }
            if (sKey == mk)
            {
                // 找到了
                if ((_pHead->_iKeyType != MainKey::ZSET_TYPE && mainKey.getHeadPtr()->_iBlockHead == 0) ||
                    (_pHead->_iKeyType == MainKey::ZSET_TYPE && mainKey.getNext(0) == 0))
                {
                    ret = RT_ONLY_KEY;
                }
                return mainKey.getHead();
            }

        } while (mainKey.next());

        return 0;
    }

    int TC_Multi_HashMap_Malloc::refreshKeyList(const string &mk, const string &uk, uint32_t unHash, bool bHead)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        if (_pHead->_bReadOnly) return RT_READONLY;

        int ret = TC_Multi_HashMap_Malloc::RT_OK;
        lock_iterator it = find(mk, uk, unHash%_hash.size(), ret);

        if (ret != TC_Multi_HashMap_Malloc::RT_OK)
        {
            return ret;
        }

        if (it == end())
        {
            // 数据尚不存在
            return TC_Multi_HashMap_Malloc::RT_NO_DATA;
        }

        Block block(this, it->getAddr());

        block.refreshKeyList(bHead);

        return TC_Multi_HashMap_Malloc::RT_OK;
    }

    size_t TC_Multi_HashMap_Malloc::count(const string &mk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);

        incGetCount();

        uint32_t iIndex = mhashIndex(mk);
        if (itemMainKey(iIndex)->_iMainKeyAddr != 0)
        {
            MainKey mainKey(this, itemMainKey(iIndex)->_iMainKeyAddr);
            do
            {
                string sKey;
                if (MainKey::ZSET_TYPE != keyType)
                    mainKey.get(sKey);
                else
                    mainKey.getData(sKey);
                if (mk == sKey)
                {
                    // 找到对应的主key了
                    incHitCount();
                    return mainKey.getHeadPtr()->_iBlockCount;
                }
            } while (mainKey.next());
        }
        return 0;
    }

    TC_Multi_HashMap_Malloc::lock_iterator TC_Multi_HashMap_Malloc::find(const string &mk)
    {
        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

        incGetCount();

        uint32_t iIndex = mhashIndex(mk);
        if (itemMainKey(iIndex)->_iMainKeyAddr != 0)
        {
            MainKey mainKey(this, itemMainKey(iIndex)->_iMainKeyAddr);
            do
            {
                string sKey;
                if (_pHead->_iKeyType == MainKey::ZSET_TYPE)
                    mainKey.getData(sKey);
                else
                    mainKey.get(sKey);

                if (mk == sKey)
                {
                    // 找到对应的主key了
                    incHitCount();
                    if (mainKey.getHeadPtr()->_iBlockHead != 0)
                    {
                        // 主key下面有数据
                        return lock_iterator(this, mainKey.getHeadPtr()->_iBlockHead, lock_iterator::IT_MKEY, lock_iterator::IT_NEXT);
                    }
                    break;
                }
            } while (mainKey.next());
        }
        return end();
    }

    void TC_Multi_HashMap_Malloc::analyseHash(uint32_t &iMaxHash, uint32_t &iMinHash, float &fAvgHash)
    {
        iMaxHash = 0;
        iMinHash = (uint32_t)-1;

        fAvgHash = 0;

        uint32_t n = 0;
        for (uint32_t i = 0; i < _hash.size(); i++)
        {
            iMaxHash = max(_hash[i]._iListCount, iMaxHash);
            iMinHash = min(_hash[i]._iListCount, iMinHash);
            //平均值只统计非0的
            if (_hash[i]._iListCount != 0)
            {
                n++;
                fAvgHash += _hash[i]._iListCount;
            }
        }

        if (n != 0)
        {
            fAvgHash = fAvgHash / n;
        }
    }

    void TC_Multi_HashMap_Malloc::analyseHashM(uint32_t &iMaxHash, uint32_t &iMinHash, float &fAvgHash)
    {
        iMaxHash = 0;
        iMinHash = (uint32_t)-1;

        fAvgHash = 0;

        uint32_t n = 0;
        for (uint32_t i = 0; i < _hashMainKey.size(); i++)
        {
            iMaxHash = max(_hashMainKey[i]._iListCount, iMaxHash);
            iMinHash = min(_hashMainKey[i]._iListCount, iMinHash);
            //平均值只统计非0的
            if (_hashMainKey[i]._iListCount != 0)
            {
                n++;
                fAvgHash += _hashMainKey[i]._iListCount;
            }
        }

        if (n != 0)
        {
            fAvgHash = fAvgHash / n;
        }
    }

    void TC_Multi_HashMap_Malloc::saveAddr(uint32_t iAddr, char cByte, bool bMainKey)
    {
        _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex]._iModifyAddr = iAddr;
        _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex]._cBytes = cByte;// 0;
        _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex]._iModifyValue = bMainKey;
        _pstCurrModify->_cModifyStatus = 1;

        _pstCurrModify->_iNowIndex++;

        assert(_pstCurrModify->_iNowIndex < sizeof(_pstCurrModify->_stModifyData) / sizeof(tagModifyData));
    }

    void TC_Multi_HashMap_Malloc::doRecover()
    {
        //==1 copy过程中, 程序失败, 恢复原数据
        /*
        if(_pstModifyHead->_cModifyStatus == 1)
        {
            for(int i = _pstModifyHead->_iNowIndex-1; i >=0; i--)
            {
                if(_pstModifyHead->_stModifyData[i]._cBytes == sizeof(uint64_t))
                {
                    *(uint64_t*)((char*)_pHead + _pstModifyHead->_stModifyData[i]._iModifyAddr) = _pstModifyHead->_stModifyData[i]._iModifyValue;
                }
                //#if __WORDSIZE == 64
                else if(_pstModifyHead->_stModifyData[i]._cBytes == sizeof(uint32_t))
                {
                    *(uint32_t*)((char*)_pHead + _pstModifyHead->_stModifyData[i]._iModifyAddr) = (uint32_t)_pstModifyHead->_stModifyData[i]._iModifyValue;
                }
                //#endif
                else if(_pstModifyHead->_stModifyData[i]._cBytes == sizeof(uint8_t))
                {
                    *(uint8_t*)((char*)_pHead + _pstModifyHead->_stModifyData[i]._iModifyAddr) = (bool)_pstModifyHead->_stModifyData[i]._iModifyValue;
                }
                else
                {
                    assert(true);
                }
            }
            _pstModifyHead->_iNowIndex        = 0;
            _pstModifyHead->_cModifyStatus    = 0;
        }

        // 如果备份区也有，同样进行恢复
        if(_pstBakModify->_cModifyStatus == 1)
        {
            for(int i = _pstBakModify->_iNowIndex-1; i >=0; i--)
            {
                if(_pstBakModify->_stModifyData[i]._cBytes == sizeof(uint64_t))
                {
                    *(uint64_t*)((char*)_pHead + _pstBakModify->_stModifyData[i]._iModifyAddr) = _pstBakModify->_stModifyData[i]._iModifyValue;
                }
                //#if __WORDSIZE == 64
                else if(_pstBakModify->_stModifyData[i]._cBytes == sizeof(uint32_t))
                {
                    *(uint32_t*)((char*)_pHead + _pstBakModify->_stModifyData[i]._iModifyAddr) = (uint32_t)_pstBakModify->_stModifyData[i]._iModifyValue;
                }
                //#endif
                else if(_pstBakModify->_stModifyData[i]._cBytes == sizeof(uint8_t))
                {
                    *(uint8_t*)((char*)_pHead + _pstBakModify->_stModifyData[i]._iModifyAddr) = (bool)_pstBakModify->_stModifyData[i]._iModifyValue;
                }
                else
                {
                    assert(true);
                }
            }
            _pstBakModify->_iNowIndex        = 0;
            _pstBakModify->_cModifyStatus    = 0;
        }
        */

        if (_pstInnerModify->_cModifyStatus == 1)
        {
            _pstCurrModify = _pstInnerModify;
            for (int i = _pstInnerModify->_iNowIndex - 1; i >= 0; i--)
            {
                if (_pstInnerModify->_stModifyData[i]._cBytes == sizeof(uint64_t))
                {
                    *(uint64_t*)((char*)_pHead + _pstInnerModify->_stModifyData[i]._iModifyAddr) = _pstInnerModify->_stModifyData[i]._iModifyValue;
                }
                //#if __WORDSIZE == 64
                else if (_pstInnerModify->_stModifyData[i]._cBytes == sizeof(uint32_t))
                {
                    *(uint32_t*)((char*)_pHead + _pstInnerModify->_stModifyData[i]._iModifyAddr) = (uint32_t)_pstInnerModify->_stModifyData[i]._iModifyValue;
                }
                //#endif
                else if (_pstInnerModify->_stModifyData[i]._cBytes == sizeof(uint8_t))
                {
                    *(uint8_t*)((char*)_pHead + _pstInnerModify->_stModifyData[i]._iModifyAddr) = (bool)_pstInnerModify->_stModifyData[i]._iModifyValue;
                }
                else if (_pstInnerModify->_stModifyData[i]._cBytes == 0)
                {
                    assert(_pstInnerModify->_stModifyData[i]._iModifyValue == (size_t)false || _pstInnerModify->_stModifyData[i]._iModifyValue == (size_t)true);
                    deallocate(_pstInnerModify->_stModifyData[i]._iModifyAddr, i, _pstInnerModify->_stModifyData[i]._iModifyValue);
                }
                else if (_pstInnerModify->_stModifyData[i]._cBytes == -1)
                {
                    continue;
                }
                else if (_pstInnerModify->_stModifyData[i]._cBytes == -2)
                {
                    assert((size_t)i == _pstInnerModify->_iNowIndex - 1);
                    assert(_pstInnerModify->_stModifyData[i]._iModifyValue == (size_t)false || _pstInnerModify->_stModifyData[i]._iModifyValue == (size_t)true);
                    deallocate2();
                    break;
                }
                else if (_pstInnerModify->_stModifyData[i]._cBytes == -3)
                {
                    assert(_pstInnerModify->_stModifyData[i]._iModifyValue == (size_t)false || _pstInnerModify->_stModifyData[i]._iModifyValue == (size_t)true);
                    deallocate3(_pstInnerModify->_stModifyData[i]._iModifyAddr, i, _pstInnerModify->_stModifyData[i]._iModifyValue);
                }
                else
                {
                    assert(true);
                }
            }
            _pstInnerModify->_iNowIndex = 0;
            _pstInnerModify->_cModifyStatus = 0;

            _pstCurrModify = _pstOuterModify;
        }

        // 修改后备份区已经没有作用，但是为了兼容之前的版本故仍然保留
        // 如果备份区也有，同样进行恢复
        if (_pstOuterModify->_cModifyStatus == 1)
        {
            for (int i = _pstOuterModify->_iNowIndex - 1; i >= 0; i--)
            {
                if (_pstOuterModify->_stModifyData[i]._cBytes == sizeof(uint64_t))
                {
                    *(uint64_t*)((char*)_pHead + _pstOuterModify->_stModifyData[i]._iModifyAddr) = _pstOuterModify->_stModifyData[i]._iModifyValue;
                }
                //#if __WORDSIZE == 64
                else if (_pstOuterModify->_stModifyData[i]._cBytes == sizeof(uint32_t))
                {
                    *(uint32_t*)((char*)_pHead + _pstOuterModify->_stModifyData[i]._iModifyAddr) = (uint32_t)_pstOuterModify->_stModifyData[i]._iModifyValue;
                }
                //#endif
                else if (_pstOuterModify->_stModifyData[i]._cBytes == sizeof(uint8_t))
                {
                    *(uint8_t*)((char*)_pHead + _pstOuterModify->_stModifyData[i]._iModifyAddr) = (bool)_pstOuterModify->_stModifyData[i]._iModifyValue;
                }
                else if (_pstOuterModify->_stModifyData[i]._cBytes == 0)
                {
                    assert(_pstOuterModify->_stModifyData[i]._iModifyValue == (size_t)false || _pstOuterModify->_stModifyData[i]._iModifyValue == (size_t)true);
                    deallocate(_pstOuterModify->_stModifyData[i]._iModifyAddr, i, _pstOuterModify->_stModifyData[i]._iModifyValue);
                }
                else if (_pstOuterModify->_stModifyData[i]._cBytes == -1)
                {
                    continue;
                }
                else if (_pstOuterModify->_stModifyData[i]._cBytes == -2)
                {
                    assert((size_t)i == _pstOuterModify->_iNowIndex - 1);
                    assert(_pstOuterModify->_stModifyData[i]._iModifyValue == (size_t)false || _pstOuterModify->_stModifyData[i]._iModifyValue == (size_t)true);
                    deallocate2();
                    break;
                }
                else if (_pstOuterModify->_stModifyData[i]._cBytes == -3)
                {
                    assert(_pstOuterModify->_stModifyData[i]._iModifyValue == (size_t)false || _pstOuterModify->_stModifyData[i]._iModifyValue == (size_t)true);
                    deallocate3(_pstOuterModify->_stModifyData[i]._iModifyAddr, i, _pstOuterModify->_stModifyData[i]._iModifyValue);
                }
                else
                {
                    assert(true);
                }
            }
            _pstOuterModify->_iNowIndex = 0;
            _pstOuterModify->_cModifyStatus = 0;
        }
    }

    void TC_Multi_HashMap_Malloc::doUpdate()
    {
        //==2,已经修改成功, 清除
        if (_pstCurrModify->_cModifyStatus == 1)
        {
            _pstCurrModify->_iNowIndex = 0;
            _pstCurrModify->_cModifyStatus = 0;
        }
    }

    void TC_Multi_HashMap_Malloc::doUpdate2()
    {
        if (_pstCurrModify->_cModifyStatus == 1)
        {
            for (size_t i = _pstCurrModify->_iNowIndex - 1; i >= 0; --i)
            {
                if (_pstCurrModify->_stModifyData[i]._cBytes == 0)
                {
                    if (_pstCurrModify->_stModifyData[i]._iModifyValue == (size_t)true || _pstCurrModify->_stModifyData[i]._iModifyValue == (size_t)false)
                    {
                        _pstCurrModify->_iNowIndex = i + 1;
                        return;
                    }
                    break;
                }
            }
        }

        assert(false);
    }

    void TC_Multi_HashMap_Malloc::doUpdate3()
    {
        if (_pstCurrModify->_cModifyStatus == 1)
        {
            if (_pstCurrModify->_iNowIndex >= 3)
            {
                tagModifyData& stModifyData = _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex - 3];
                if (stModifyData._cBytes == -1 && (stModifyData._iModifyValue == (size_t)false || stModifyData._iModifyValue == (size_t)true))
                {
                    saveValue(&stModifyData._cBytes, 0);
                    _pstCurrModify->_iNowIndex -= 3;
                    deallocate(stModifyData._iModifyAddr, _pstCurrModify->_iNowIndex - 1, stModifyData._iModifyValue);
                    if (_pstCurrModify->_iNowIndex == 1)
                    {
                        _pstCurrModify->_iNowIndex = 0;
                        _pstCurrModify->_cModifyStatus = 0;
                    }
                    else
                        _pstCurrModify->_iNowIndex -= 1;

                    return;
                }
            }
        }

        assert(false);
    }

    void TC_Multi_HashMap_Malloc::doUpdate4()
    {
        if (_pstCurrModify->_cModifyStatus == 1)
        {
            if (_pstCurrModify->_iNowIndex >= 1)
            {
                tagModifyData& stModifyData = _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex - 1];
                if (stModifyData._cBytes == -2 && (stModifyData._iModifyValue == (size_t)false || stModifyData._iModifyValue == (size_t)true))
                {
                    deallocate2();
                    return;
                }
            }
        }

        assert(false);
    }

    size_t TC_Multi_HashMap_Malloc::getMinPrimeNumber(size_t n)
    {
        while (true)
        {
            if (TC_Common::isPrimeNumber(n))
            {
                return n;
            }
            ++n;
        }
        return 3;
    }

    size_t TC_Multi_HashMap_Malloc::checkBadBlock(uint32_t iHash, bool bRepair)
    {
        size_t iCount = 0;
        if (iHash >= _hash.size())
        {
            return 0;
        }

        TC_Multi_HashMap_Malloc::FailureRecover recover(this);

    check:
        uint32_t iAddr = item(iHash)->_iBlockAddr;
        Block block(this, iAddr);
        while (block.getHead() != 0)
        {
            BlockData blkData;
            int ret = block.getBlockData(blkData);
            if (ret != RT_OK && ret != RT_ONLY_KEY)
            {
                iCount++;
                if (bRepair)
                {
                    // 删除这个block
                    block.erase();
                    goto check;
                }
            }
            if (!block.nextBlock())
            {
                break;
            }
        }

        return iCount;
    }

    void TC_Multi_HashMap_Malloc::deallocate(uint32_t iChunk, size_t iIndex, bool bMainKey)
    {
        BlockAllocator* pAllocator = _pDataAllocator;
        MemChunk chunk, chunkFirst;
        chunkFirst._iAddr = iChunk;
        if (bMainKey)
        {
            pAllocator = _pMainKeyAllocator;

            MainKey::tagChunkHead *pChunk = (MainKey::tagChunkHead*)getMainKeyAbsolute(iChunk);
            chunkFirst._iSize = pChunk->_iSize;

            //逐个释放掉chunk
            while (pChunk->_bNextChunk)
            {
                //注意顺序，保证即使程序这时再退出也最多是丢失一个chunk
                uint32_t iNextChunk = pChunk->_iNextChunk;
                MainKey::tagChunkHead *pNextChunk = (MainKey::tagChunkHead*)getMainKeyAbsolute(iNextChunk);
                chunk._iAddr = iNextChunk;
                chunk._iSize = pNextChunk->_iSize;
                pChunk->_bNextChunk = pNextChunk->_bNextChunk;
                pChunk->_iNextChunk = pNextChunk->_iNextChunk;

                pAllocator->deallocateMemChunk(chunk, true);
            }
        }
        else
        {
            Block::tagChunkHead *pChunk = (Block::tagChunkHead*)getAbsolute(iChunk);
            chunkFirst._iSize = pChunk->_iSize;

            //逐个释放掉chunk
            while (pChunk->_bNextChunk)
            {
                //注意顺序，保证即使程序这时再退出也最多是丢失一个chunk
                uint32_t iNextChunk = pChunk->_iNextChunk;
                Block::tagChunkHead *pNextChunk = (Block::tagChunkHead*)getAbsolute(iNextChunk);
                chunk._iAddr = iNextChunk;
                chunk._iSize = pNextChunk->_iSize;
                pChunk->_bNextChunk = pNextChunk->_bNextChunk;
                pChunk->_iNextChunk = pNextChunk->_iNextChunk;

                pAllocator->deallocateMemChunk(chunk, false);
            }
        }

        _pstCurrModify->_stModifyData[iIndex]._cBytes = -1;
        pAllocator->deallocateMemChunk(chunkFirst, bMainKey);
    }

    void TC_Multi_HashMap_Malloc::deallocate2()
    {
        MainKey::KEYTYPE keyType = (MainKey::KEYTYPE)(_pHead->_iKeyType);

        uint32_t iHead = _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex - 1]._iModifyAddr;

        bool bMainKey = _pstCurrModify->_stModifyData[_pstCurrModify->_iNowIndex - 1]._iModifyValue;

        BlockAllocator* pAllocator = _pDataAllocator;
        MemChunk chunk, chunkFirst;
        chunkFirst._iAddr = iHead;
        if (bMainKey)
        {
            pAllocator = _pMainKeyAllocator;

            MainKey::tagMainKeyHead *pHead = (MainKey::tagMainKeyHead*)getMainKeyAbsolute(iHead);
            chunkFirst._iSize = pHead->_iSize;

            //逐个释放掉chunk
            while (ISSET(pHead->_iBitset, MainKey::NEXTCHUNK_BIT))
            {
                //注意顺序，保证即使程序这时再退出也最多是丢失一个chunk
                uint32_t iNextChunk = pHead->_iNextChunk;
                MainKey::tagChunkHead *pNextChunk = (MainKey::tagChunkHead*)getMainKeyAbsolute(iNextChunk);
                chunk._iAddr = iNextChunk;
                chunk._iSize = pNextChunk->_iSize;
                if (pNextChunk->_bNextChunk)
                {
                    pHead->_iNextChunk = pNextChunk->_iNextChunk;
                }
                else
                    UNSET(pHead->_iBitset, MainKey::NEXTCHUNK_BIT);

                pAllocator->deallocateMemChunk(chunk, true);
            }
        }
        else if (MainKey::LIST_TYPE != keyType)
        {
            Block::tagBlockHead *pHead = (Block::tagBlockHead*)getAbsolute(iHead);
            chunkFirst._iSize = pHead->_iSize;

            //逐个释放掉chunk
            while (ISSET(pHead->_iBitset, Block::NEXTCHUNK_BIT))
            {
                //注意顺序，保证即使程序这时再退出也最多是丢失一个chunk
                uint32_t iNextChunk = pHead->_iNextChunk;
                Block::tagChunkHead *pNextChunk = (Block::tagChunkHead*)getAbsolute(iNextChunk);
                chunk._iAddr = iNextChunk;
                chunk._iSize = pNextChunk->_iSize;
                if (pNextChunk->_bNextChunk)
                {
                    pHead->_iNextChunk = pNextChunk->_iNextChunk;
                }
                else
                    UNSET(pHead->_iBitset, Block::NEXTCHUNK_BIT);

                pAllocator->deallocateMemChunk(chunk, false);
            }
        }
        else
        {
            Block::tagListBlockHead *pHead = (Block::tagListBlockHead*)getAbsolute(iHead);
            chunkFirst._iSize = pHead->_iSize;

            //逐个释放掉chunk
            while (ISSET(pHead->_iBitset, Block::NEXTCHUNK_BIT))
            {
                //注意顺序，保证即使程序这时再退出也最多是丢失一个chunk
                uint32_t iNextChunk = pHead->_iNextChunk;
                Block::tagChunkHead *pNextChunk = (Block::tagChunkHead*)getAbsolute(iNextChunk);
                chunk._iAddr = iNextChunk;
                chunk._iSize = pNextChunk->_iSize;
                if (pNextChunk->_bNextChunk)
                {
                    pHead->_iNextChunk = pNextChunk->_iNextChunk;
                }
                else
                    UNSET(pHead->_iBitset, Block::NEXTCHUNK_BIT);

                pAllocator->deallocateMemChunk(chunk, false);
            }
        }

        _pstCurrModify->_iNowIndex = 0;
        _pstCurrModify->_cModifyStatus = 0;
        pAllocator->deallocateMemChunk(chunkFirst, bMainKey);
    }

    void TC_Multi_HashMap_Malloc::deallocate3(uint32_t iHead, size_t iIndex, bool bMainKey)
    {
        BlockAllocator* pAllocator = _pDataAllocator;
        MemChunk chunk;
        chunk._iAddr = iHead;

        if (bMainKey)
        {
            pAllocator = _pMainKeyAllocator;
            chunk._iSize = ((MainKey::tagMainKeyHead*)getMainKeyAbsolute(iHead))->_iSize;
        }
        else
            chunk._iSize = ((Block::tagBlockHead*)getAbsolute(iHead))->_iSize;

        _pstCurrModify->_stModifyData[iIndex]._cBytes = -1;
        pAllocator->deallocateMemChunk(chunk, bMainKey);
    }

}


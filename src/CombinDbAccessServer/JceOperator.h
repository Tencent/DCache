#ifndef _JCEOPERATOR_H
#define _JCEOPERATOR_H
//#include "StringUtil.h"
#include "util/tc_common.h"
#include <string>
// #include "wup-linux-c++/Jce.h"
#include "tup/Tars.h"
/*
 * 封装jce编码二进制流
 */

using namespace std;
using namespace tars;

class JceEncode
{
public:
	JceEncode(){}
	~JceEncode(){}

	const char* getBuffer()
	{
		return m_osk.getBuffer();
	}

	size_t getLength()
	{
		return m_osk.getLength();
	}

	/*
	 * 返回jce编码二进制流
	 */
	/*
	const vector<char>& getBuffer()
	{
		return m_osk.getByteBuffer();
	}
	*/

	/*
     * 将某字段写入jce编码
     * @param s: 某字段的值
	 * @param tag: 某字段的tag编号
	 * @param type: 某字段的类型
     */
	void write(const string &s, uint8_t tag, string type);

private:
	TarsOutputStream<BufferWriter> m_osk;
};

/*
 * 解码jce编码二进制流
 */
class JceDecode
{
public:
	JceDecode(){}
	~JceDecode(){}
	void setBuffer(const string &sBuf)
    {
        m_sBuf = sBuf;
    }
	string getBuffer()
    {
        return m_sBuf;
    }

	/*
     * 将某字段从jce编码流中读出
	 * @param tag: 某字段的tag编号
	 * @param type: 某字段的类型
	 * @param sDefault: 默认值
	 * @param isRequire: 是否是必须字段
     */
	string read(uint8_t tag, const string& type, const string &sDefault, bool isRequire);

private:
	string m_sBuf;
};

#endif

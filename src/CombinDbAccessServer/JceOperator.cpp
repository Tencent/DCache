#include "JceOperator.h"

#define JCE_BYTE "byte"
#define JCE_SHORT "short"
#define JCE_INT "int"
#define JCE_LONG "long"
#define JCE_STRING "string"
#define JCE_FLOAT "float"
#define JCE_DOUBLE "double"
#define JCE_UINT32 "unsigned int"
#define JCE_UINT16 "unsigned short"
//#define JCE_UINT8  "unsigned byte"

void JceEncode::write(const string &s, uint8_t tag, string type)
{
	if(type == JCE_BYTE)
	{
		tars::Char n = TC_Common::strto<tars::Char>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_SHORT)
	{
		tars::Short n = TC_Common::strto<tars::Short>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_INT)
	{
		tars::Int32 n = TC_Common::strto<tars::Int32>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_LONG)
	{
		tars::Int64 n = TC_Common::strto<tars::Int64>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_STRING)
	{
		string n = TC_Common::strto<string>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_FLOAT)
	{
		tars::Float n = TC_Common::strto<tars::Float>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_DOUBLE)
	{
		tars::Double n = TC_Common::strto<tars::Double>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_UINT32)
	{
		tars::UInt32 n = TC_Common::strto<tars::UInt32>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_UINT16)
	{
		tars::UInt16 n = TC_Common::strto<tars::UInt16>(s);
		m_osk.write(n, tag);
	}
	else
	{
		throw runtime_error("JceEncode::write type error");
	}
}
string JceDecode::read(uint8_t tag, const string& type, const string &sDefault, bool isRequire)
{
	TarsInputStream<BufferReader> isk;
	isk.setBuffer(m_sBuf.c_str(), m_sBuf.length());
	string s;

	if(type == JCE_BYTE)
	{
		tars::Char n;
		n = TC_Common::strto<tars::Char>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_SHORT)
	{
		tars::Short n;
		n = TC_Common::strto<tars::Short>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_INT)
	{
		tars::Int32 n;
		n = TC_Common::strto<tars::Int32>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_LONG)
	{
		tars::Int64 n;
		n = TC_Common::strto<tars::Int64>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_STRING)
	{
		s = sDefault;
		isk.read(s, tag, isRequire);
	}
	else if(type == JCE_FLOAT)
	{
		tars::Float n;
		n = TC_Common::strto<tars::Float>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_DOUBLE)
	{
		tars::Double n;
		n = TC_Common::strto<tars::Double>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_UINT32)
	{
		tars::UInt32 n;
		n = TC_Common::strto<tars::UInt32>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_UINT16)
	{
		tars::UInt16 n;
		n = TC_Common::strto<tars::UInt16>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else
	{
		throw runtime_error("JceEncode::read type error");
	}
	return s;
}

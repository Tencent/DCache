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
		taf::Char n = TC_Common::strto<taf::Char>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_SHORT)
	{
		taf::Short n = TC_Common::strto<taf::Short>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_INT)
	{
		taf::Int32 n = TC_Common::strto<taf::Int32>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_LONG)
	{
		taf::Int64 n = TC_Common::strto<taf::Int64>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_STRING)
	{
		string n = TC_Common::strto<string>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_FLOAT)
	{
		taf::Float n = TC_Common::strto<taf::Float>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_DOUBLE)
	{
		taf::Double n = TC_Common::strto<taf::Double>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_UINT32)
	{
		taf::UInt32 n = TC_Common::strto<taf::UInt32>(s);
		m_osk.write(n, tag);
	}
	else if(type == JCE_UINT16)
	{
		taf::UInt16 n = TC_Common::strto<taf::UInt16>(s);
		m_osk.write(n, tag);
	}
	else
	{
		throw runtime_error("JceEncode::write type error");
	}
}
string JceDecode::read(uint8_t tag, const string& type, const string &sDefault, bool isRequire)
{
	JceInputStream<BufferReader> isk;
	isk.setBuffer(m_sBuf.c_str(), m_sBuf.length());
	string s;

	if(type == JCE_BYTE)
	{
		taf::Char n;
		n = TC_Common::strto<taf::Char>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_SHORT)
	{
		taf::Short n;
		n = TC_Common::strto<taf::Short>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_INT)
	{
		taf::Int32 n;
		n = TC_Common::strto<taf::Int32>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_LONG)
	{
		taf::Int64 n;
		n = TC_Common::strto<taf::Int64>(sDefault);
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
		taf::Float n;
		n = TC_Common::strto<taf::Float>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_DOUBLE)
	{
		taf::Double n;
		n = TC_Common::strto<taf::Double>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_UINT32)
	{
		taf::UInt32 n;
		n = TC_Common::strto<taf::UInt32>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else if(type == JCE_UINT16)
	{
		taf::UInt16 n;
		n = TC_Common::strto<taf::UInt16>(sDefault);
		isk.read(n, tag, isRequire);
		s = TC_Common::tostr(n);
	}
	else
	{
		throw runtime_error("JceEncode::read type error");
	}
	return s;
}

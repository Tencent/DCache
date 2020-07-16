#ifndef _CombinDbAccessServer_H_
#define _CombinDbAccessServer_H_

#include <iostream>
#include "util/tc_config.h"
#include "servant/Application.h"
using namespace taf;

/**
 *
 **/
class CombinDbAccessServer : public Application
{
public:
	/**
	 *
	 **/
	virtual ~CombinDbAccessServer() {};

	/**
	 *
	 **/
	virtual void initialize();

	/**
	 *
	 **/
	virtual void destroyApp();
	
	bool showVer(const string& command, const string& params, string& result);
};

extern CombinDbAccessServer g_app;

////////////////////////////////////////////
#endif

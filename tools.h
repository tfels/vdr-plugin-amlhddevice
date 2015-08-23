/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef TOOLS_H
#define TOOLS_H

#include <vdr/tools.h>

#define ELOG(a...) esyslog("amlhddevice: " a)
#define ILOG(a...) isyslog("amlhddevice: " a)
#define DLOG(a...) dsyslog("amlhddevice: " a)

#ifdef DEBUG
#define DBG(a...)  dsyslog("amlhddevice: " a)
#else
#define DBG(a...)  void()
#endif

class cSysFs
{

public:

	static bool Write(const char *path, const char *value);
	static bool Write(const char *path, int value);

private:

	cSysFs();
};

#endif

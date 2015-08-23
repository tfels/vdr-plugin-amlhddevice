/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef FB_OSD_H
#define FB_OSD_H

#include <vdr/osd.h>

class cFbOsdProvider : public cOsdProvider
{

public:

	cFbOsdProvider(const char* device);
	~cFbOsdProvider();

protected:

	virtual cOsd *CreateOsd(int Left, int Top, uint Level);
	virtual bool ProvidesTrueColor(void) { return true; }

private:

	const char* m_device;
};

#endif

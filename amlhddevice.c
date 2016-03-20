/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <vdr/plugin.h>

#include "amldevice.h"
#include "fbosd.h"
#include "tools.h"

static const char *VERSION     = "0.0.1";
static const char *DESCRIPTION = trNOOP("HD output device for Amlogic SoC");

class cPluginAmlHdDevice : public cPlugin
{
private:

	cAmlDevice *m_device;

	static void OnPrimaryDevice(void)
	{
		new cFbOsdProvider("/dev/fb1");
	}

public:
	cPluginAmlHdDevice(void);
	virtual ~cPluginAmlHdDevice();

	virtual const char *Version(void) { return VERSION; }
	virtual const char *Description(void) { return tr(DESCRIPTION); }
	virtual const char *CommandLineHelp(void) { return NULL; }
	virtual bool ProcessArgs(int argc, char *argv[]) { return true; }
	virtual bool Initialize(void);
	virtual bool Start(void);
	virtual void Stop(void);
	virtual void Housekeeping(void) {}
	virtual const char *MainMenuEntry(void) { return NULL; }
	virtual cOsdObject *MainMenuAction(void) { return NULL; }
	virtual cMenuSetupPage *SetupMenu(void) { return NULL; }
	virtual bool SetupParse(const char *Name, const char *Value) { return false; }
};

cPluginAmlHdDevice::cPluginAmlHdDevice(void) :
	m_device(0)
{
}

cPluginAmlHdDevice::~cPluginAmlHdDevice()
{
}

bool cPluginAmlHdDevice::Initialize(void)
{
	m_device = new cAmlDevice(&OnPrimaryDevice);

	if (m_device)
	{
		if (m_device->Init())
			return false;

		cSysFs::Write("/sys/class/graphics/fb1/mode", "U:1280x720p-0\n");
		cSysFs::Write("/sys/class/graphics/fb1/blank", 0);
		cSysFs::Write("/sys/class/graphics/fb0/blank", 1);
	}
	return true;
}

bool cPluginAmlHdDevice::Start(void)
{
	return m_device->Start();
}

void cPluginAmlHdDevice::Stop(void)
{
	cSysFs::Write("/sys/class/graphics/fb1/blank", 1);
	cSysFs::Write("/sys/class/graphics/fb0/blank", 0);
}

VDRPLUGINCREATOR(cPluginAmlHdDevice); // Don't touch this! okay.

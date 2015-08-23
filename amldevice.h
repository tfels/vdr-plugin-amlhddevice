/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef AML_DEVICE_H
#define AML_DEVICE_H

#include <vdr/device.h>

#include "tools.h"

class cAmlAudioDecoder;
class cAmlVideoDecoder;

class cAmlDevice : cDevice
{

public:

	cAmlDevice(void (*onPrimaryDevice)(void));
	virtual ~cAmlDevice();

	virtual int Init(void);
	virtual int DeInit(void);

	virtual bool Start(void);

	virtual bool HasDecoder(void) const { return true; }

	virtual bool SetPlayMode(ePlayMode PlayMode);

	virtual int PlayVideo(const uchar *Data, int Length);
	virtual int PlayAudio(const uchar *Data, int Length, uchar Id);

	virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
	virtual bool Flush(int TimeoutMs = 0);

	virtual void GetOsdSize(int &Width, int &Height, double &PixelAspect);

protected:

	virtual void MakePrimaryDevice(bool On);

	virtual int PlayTsVideo(const uchar *Data, int Length);
	virtual int PlayTsAudio(const uchar *Data, int Length);

private:

	void (*m_onPrimaryDevice)(void);

	cAmlAudioDecoder *m_audioDecoder;
	cAmlVideoDecoder *m_videoDecoder;

	int m_audioPid;
	int m_videoPid;
};

#endif

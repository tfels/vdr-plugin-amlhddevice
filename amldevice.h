/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef AML_DEVICE_H
#define AML_DEVICE_H

#include <vdr/device.h>
#include "tools.h"

extern "C" {
#include <codec.h>
}

class cScheduler
{

public:

	cScheduler();
	virtual ~cScheduler() { }

	void SetSpeed(int speed, bool forward);
	void Reset();

	bool Check(int64_t pts);
	int64_t GetPts(void);

	enum eSpeed {
		ePause,
		eSlowest,
		eSlower,
		eSlow,
		eNormal,
		eFast,
		eFaster,
		eFastest,
		eNumSpeeds
	};

private:

	int64_t m_pts;
	cTimeMs m_timestamp;

	eSpeed m_speed;
	bool m_forward;

	static const int s_speeds[eNumSpeeds];
};

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

	virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
	virtual bool Flush(int TimeoutMs = 0);

	virtual void GetOsdSize(int &Width, int &Height, double &PixelAspect);

	virtual int64_t GetSTC(void);

protected:

	virtual void MakePrimaryDevice(bool On);

	virtual int PlayVideo(const uchar *Data, int Length);
	virtual int PlayAudio(const uchar *Data, int Length, uchar Id);

	void StillPicture(const uchar *Data, int Length);

	virtual void Clear(void);
	virtual void Play(void);
	virtual void Freeze(void);

	virtual void TrickSpeed(int Speed, bool Forward);

	virtual void SetVolumeDevice(int Volume);

private:

	void (*m_onPrimaryDevice)(void);

	codec_para_t m_videoCodec;
	codec_para_t m_audioCodec;

	uchar m_audioId;
	bool m_trickMode;

	cScheduler m_scheduler;
};

#endif

/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <stdio.h>

extern "C" {
#include <codec.h>
}

#include <libsi/si.h>

#include "tools.h"
#include "amldevice.h"

#define INVALID_PTS (-1)

/* ------------------------------------------------------------------------- */

class cAmlDecoder
{
public:

	cAmlDecoder() :
		m_initialized(false)
	{
		Reset();
	}

	virtual ~cAmlDecoder()
	{
		Stop();
	}

	virtual void Reset(void)
	{
		Stop();
		memset(&m_param, 0, sizeof(codec_para_t));
		m_param.stream_type = STREAM_TYPE_TS;
	}

	virtual int WriteTs(const uchar *data, int length)
	{
		int ret = length;

		while (length)
		{
			int written = codec_write(&m_param, (void *)data, length);
			if (written > 0)
			{
				length -= written;
				data += written;
			}
			else
			{
				ELOG("failed to write codec data, ret=0x%x", written);
				ret = 0;
				break;
			}
		}
		return ret;
	}

	void SetAudioPid(int pid, int streamType)
	{
		Stop();

		m_param.has_audio = 1;
		m_param.audio_pid = pid;
		m_param.audio_type =
				streamType == 0x03 ? AFORMAT_MPEG :
				streamType == 0x04 ? AFORMAT_MPEG :
				streamType == SI::AC3DescriptorTag ? AFORMAT_AC3 :
				streamType == SI::EnhancedAC3DescriptorTag ? AFORMAT_EAC3 :
				streamType == 0x0f ? AFORMAT_AAC :
				streamType == 0x11 ? AFORMAT_AAC : AFORMAT_UNKNOWN;

		m_param.audio_channels = 0;
		m_param.audio_samplerate = 0;
		m_param.audio_info.channels = 0;
		m_param.audio_info.sample_rate = 0;

		Start();
	}

	void SetVideoPid(int pid, int streamType)
	{
		Stop();

		m_param.has_video = 1;
		m_param.video_pid = pid;
		m_param.video_type =
				streamType == 0x01 ? VFORMAT_MPEG12 :
				streamType == 0x02 ? VFORMAT_MPEG12 :
				streamType == 0x1b ? VFORMAT_H264   : VFORMAT_UNKNOWN;

		m_param.am_sysinfo.format = m_param.video_type == VFORMAT_H264 ?
				VIDEO_DEC_FORMAT_H264 : VIDEO_DEC_FORMAT_UNKNOW;

		Start();
	}

protected:

	virtual void Stop(void)
	{
		if (!m_initialized || (!m_param.has_audio && !m_param.has_video))
			return;

		if (codec_close(&m_param) != CODEC_ERROR_NONE)
			ELOG("failed to close codec!");
		else
			m_initialized = false;
	}

	virtual void Start(void)
	{
		if (m_initialized || (!m_param.has_audio && !m_param.has_video))
			return;

		cSysFs::Write("/sys/class/tsdemux/stb_source", 2);
		cSysFs::Write("/sys/class/tsync/pts_pcrscr", "0x0");

		if (codec_init(&m_param) != CODEC_ERROR_NONE)
			ELOG("failed to init codec!");
		else
		{
			m_initialized = true;
			cSysFs::Write("/sys/class/tsync/enable",
					m_param.has_audio && m_param.has_video ? 1 : 0);
		}
	}

	codec_para_t m_param;
	bool m_initialized;
};

/* ------------------------------------------------------------------------- */

cAmlDevice::cAmlDevice(void (*onPrimaryDevice)(void)) :
	cDevice(),
	m_onPrimaryDevice(onPrimaryDevice),
	m_decoder(new cAmlDecoder()),
	m_audioPid(0),
	m_videoPid(0)
{
}

cAmlDevice::~cAmlDevice()
{
	DeInit();

	delete m_decoder;
}

int cAmlDevice::Init(void)
{
	DBG("%s", __FUNCTION__);
	return 0;
}

int cAmlDevice::DeInit(void)
{
	DBG("%s", __FUNCTION__);
	return 0;
}

bool cAmlDevice::Start(void)
{
	DBG("%s", __FUNCTION__);
	return true;
}

void cAmlDevice::MakePrimaryDevice(bool On)
{
	DBG("%s", __FUNCTION__);

	if (On && m_onPrimaryDevice)
		m_onPrimaryDevice();
	cDevice::MakePrimaryDevice(On);

	codec_audio_basic_init();
}

void cAmlDevice::GetOsdSize(int &Width, int &Height, double &PixelAspect)
{
	Width = 1280;
	Height = 720;
	PixelAspect = (double)Width / Height;
}

bool cAmlDevice::SetPlayMode(ePlayMode PlayMode)
{
	DBG("%s(%s)", __FUNCTION__,
		PlayMode == pmNone			 ? "none" 			   :
		PlayMode == pmAudioVideo	 ? "Audio/Video" 	   :
		PlayMode == pmAudioOnly		 ? "Audio only" 	   :
		PlayMode == pmAudioOnlyBlack ? "Audio only, black" :
		PlayMode == pmVideoOnly		 ? "Video only" 	   :
									   "unsupported");
	switch (PlayMode)
	{
	case pmNone:
		m_decoder->Reset();
		m_audioPid = 0;
		m_videoPid = 0;
		break;

	case pmAudioVideo:
	case pmAudioOnly:
	case pmAudioOnlyBlack:
	case pmVideoOnly:
		break;

	default:
		break;
	}

	return true;
}

int cAmlDevice::PlayTsVideo(const uchar *Data, int Length)
{
	int pid = TsPid(Data);
	if (pid != m_videoPid)
	{
		PatPmtParser();
		if (pid == PatPmtParser()->Vpid())
		{
			m_decoder->SetVideoPid(pid, PatPmtParser()->Vtype());
			m_videoPid = pid;
		}
	}
	return m_decoder->WriteTs(Data, Length);
}

int cAmlDevice::PlayTsAudio(const uchar *Data, int Length)
{
	int pid = TsPid(Data);
	if (pid != m_audioPid)
	{
		int streamType = -1;
		for (int i = 0; PatPmtParser()->Apid(i); i++)
			if (pid == PatPmtParser()->Apid(i))
			{
				streamType = PatPmtParser()->Atype(i);
				break;
			}
		if (streamType < 0)
			for (int i = 0; PatPmtParser()->Dpid(i); i++)
				if (pid == PatPmtParser()->Dpid(i))
				{
					streamType = PatPmtParser()->Dtype(i);
					break;
				}

		m_decoder->SetAudioPid(pid, streamType);
		m_audioPid = pid;
	}
	return m_decoder->WriteTs(Data, Length);
}

bool cAmlDevice::Poll(cPoller &Poller, int TimeoutMs)
{
	return true;
}

bool cAmlDevice::Flush(int TimeoutMs)
{
	DBG("%s", __FUNCTION__);
	return true;
}

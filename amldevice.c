/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <stdio.h>

extern "C" {
#include <codec.h>
}

#include <vdr/libsi/si.h>

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
		memset(&m_param, 0, sizeof(codec_para_t));
	}

	virtual ~cAmlDecoder()
	{
		if (m_initialized)
			codec_close(&m_param);
	}

	virtual void Reset(void)
	{
		if (m_initialized)
			codec_close(&m_param);

		m_initialized = false;
	}

	virtual int Write(const uchar *data, int length, int64_t pts = INVALID_PTS)
	{
		if (!m_initialized)
			return 0;

		int ret = length;

		if (pts != INVALID_PTS)
			codec_checkin_pts(&m_param, pts);

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

	virtual bool Initialized(void)
	{
		return m_initialized;
	}

protected:

	bool ApplyConfig(void)
	{
		int ret = m_initialized ? codec_reset(&m_param) : codec_init(&m_param);
		m_initialized = ret == CODEC_ERROR_NONE;
		return m_initialized;
	}

	codec_para_t m_param;
	bool m_initialized;
};

/* ------------------------------------------------------------------------- */

class cAmlAudioDecoder : public cAmlDecoder
{
public:

	enum eAudioCodec {
		eUnknown = 0,
		eMPEG,
		eAAC,
		eAC3,
		eEAC3,
		eDTS
	};

	static const char* Str(eAudioCodec codec) {
		return	codec == eMPEG ? "MPEG" :
				codec == eAAC  ? "AAC"  :
				codec == eAC3  ? "AC3"  :
				codec == eEAC3 ? "EAC3" :
				codec == eDTS  ? "DTS"  : "unknown";
	}

	cAmlAudioDecoder() :
		cAmlDecoder()
	{
		codec_audio_basic_init();
	}

	void SetCodec(eAudioCodec codec)
	{
		m_param.has_audio = 1;
		m_param.stream_type = STREAM_TYPE_ES_AUDIO;
		m_param.audio_type =
				codec == eMPEG ? AFORMAT_MPEG :
				codec == eAAC  ? AFORMAT_AAC  :
				codec == eAC3  ? AFORMAT_AC3  :
				codec == eEAC3 ? AFORMAT_EAC3 :
				codec == eDTS  ? AFORMAT_DTS  : 0;

		m_param.audio_channels = 0;
		m_param.audio_samplerate = 0;
		m_param.audio_info.channels = 0;
		m_param.audio_info.sample_rate = 0;

		if (!ApplyConfig())
			ELOG("failed to set audio codec to %s!", Str(codec));
		else
			DLOG("set audio codec to %s", Str(codec));
	}

	static eAudioCodec MapStreamTypes(int type)
	{
		return	type == 0x03 ? eMPEG :
				type == 0x04 ? eMPEG :
				type == SI::AC3DescriptorTag ? eAC3 :
				type == SI::EnhancedAC3DescriptorTag ? eEAC3 :
				type == 0x0f ? eAAC :
				type == 0x11 ? eAAC : eUnknown;
	}
};

/* ------------------------------------------------------------------------- */

class cAmlVideoDecoder : public cAmlDecoder
{

#define EXTERNAL_PTS (1)
#define SYNC_OUTSIDE (2)

public:

	enum eVideoCodec {
		eUnknown = 0,
		eMPEG12,
		eH264
	};

	static const char* Str(eVideoCodec codec) {
		return	codec == eMPEG12 ? "MPEG1/2" :
				codec == eH264   ? "H264"    : "unknown";
	}

	void SetCodec(eVideoCodec codec)
	{
		m_param.has_video = 1;
		m_param.stream_type = STREAM_TYPE_ES_VIDEO;
		m_param.video_type =
				codec == eMPEG12 ? VFORMAT_MPEG12 :
				codec == eH264   ? VFORMAT_H264   : 0;

		m_param.am_sysinfo.format = VIDEO_DEC_FORMAT_UNKNOW;
		m_param.am_sysinfo.param = (void *)(EXTERNAL_PTS | SYNC_OUTSIDE);
		m_param.am_sysinfo.rate = 0;

		if (!ApplyConfig())
			ELOG("failed to set video codec to %s!", Str(codec));
		else
			DLOG("set video codec to %s", Str(codec));
	}

	static eVideoCodec MapStreamTypes(int type)
	{
		return type == 0x01 ? eMPEG12 :
				type == 0x02 ? eMPEG12 :
				type == 0x1b ? eH264 : eUnknown;
	}
};

/* ------------------------------------------------------------------------- */

cAmlDevice::cAmlDevice(void (*onPrimaryDevice)(void)) :
	cDevice(),
	m_onPrimaryDevice(onPrimaryDevice),
	m_audioDecoder(new cAmlAudioDecoder()),
	m_videoDecoder(new cAmlVideoDecoder()),
	m_audioPid(0),
	m_videoPid(0)
{
}

cAmlDevice::~cAmlDevice()
{
	DeInit();

	delete m_videoDecoder;
	delete m_audioDecoder;
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
		m_audioDecoder->Reset();
		m_videoDecoder->Reset();
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

int cAmlDevice::PlayVideo(const uchar *Data, int Length)
{
	return m_videoDecoder->Write(
			Data + PesPayloadOffset(Data),
			Length - PesPayloadOffset(Data),
			PesHasPts(Data) ? PesGetPts(Data) : INVALID_PTS);
}

int cAmlDevice::PlayAudio(const uchar *Data, int Length, uchar Id)
{
	return m_audioDecoder->Write(
			Data + PesPayloadOffset(Data),
			Length - PesPayloadOffset(Data),
			PesHasPts(Data) ? PesGetPts(Data) : INVALID_PTS);
}

int cAmlDevice::PlayTsVideo(const uchar *Data, int Length)
{
	int pid = TsPid(Data);
	if (pid != m_videoPid)
	{
		PatPmtParser();
		if (pid == PatPmtParser()->Vpid())
		{
			m_videoDecoder->SetCodec(cAmlVideoDecoder::MapStreamTypes(
					PatPmtParser()->Vtype()));
			m_videoPid = pid;
		}
	}
	return cDevice::PlayTsVideo(Data, Length);
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
		m_audioDecoder->SetCodec(cAmlAudioDecoder::MapStreamTypes(streamType));
		m_audioPid = pid;
	}
	return cDevice::PlayTsAudio(Data, Length);
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

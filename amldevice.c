/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <stdio.h>
#include <libsi/si.h>

#include "tools.h"
#include "amldevice.h"

#define INVALID_PTS (-1)

#define TRICKMODE_NONE (0)
#define TRICKMODE_I    (1)
#define TRICKMODE_FFFB (2)

#define EXTERNAL_PTS   (1)
#define SYNC_OUTSIDE   (2)

#define PTS_FREQ       90000
#define PTS_FREQ_MS    PTS_FREQ / 1000
#define AV_SYNC_THRESH PTS_FREQ * 30

/* ------------------------------------------------------------------------- */

const int cScheduler::s_speeds[eNumSpeeds] = {
	0, PTS_FREQ_MS / 12, PTS_FREQ_MS / 8, PTS_FREQ_MS / 4,
	PTS_FREQ_MS, PTS_FREQ_MS * 4, PTS_FREQ_MS * 8, PTS_FREQ_MS * 12
};

cScheduler::cScheduler() :
	m_pts(INVALID_PTS),
	m_timestamp(),
	m_speed(eNormal),
	m_forward(true)
{ }

void cScheduler::SetSpeed(int speed, bool forward)
{
	m_speed =

		// slow forward
		speed ==  8 ? eSlowest :
		speed ==  4 ? eSlower  :
		speed ==  2 ? eSlow    :

		// fast for-/backward
		speed ==  6 ? eFast    :
		speed ==  3 ? eFaster  :
		speed ==  1 ? eFastest :

		// slow backward
		speed == 63 ? eSlowest :
		speed == 48 ? eSlower  :
		speed == 24 ? eSlow    : eNormal;

	m_forward = forward;
}

void cScheduler::Reset()
{
	m_pts = INVALID_PTS;
	m_speed = eNormal;
}

bool cScheduler::Check(int64_t pts)
{
	// no scheduling
	if (m_speed == eNormal || pts == INVALID_PTS)
		return true;

	// first packet
	if (m_pts == INVALID_PTS)
	{
		m_timestamp.Set(0);
		m_pts = pts;
		return true;
	}

	if ((int64_t)m_timestamp.Elapsed() * s_speeds[m_speed] >
		(m_forward ? PtsDiff(m_pts, pts) : PtsDiff(pts, m_pts)))
	{
		m_pts = pts;
		m_timestamp.Set(0);
		return true;
	}
	return false;
}

int64_t cScheduler::GetPts(void)
{
	return m_pts;
}

/* ------------------------------------------------------------------------- */

cAmlDevice::cAmlDevice(void (*onPrimaryDevice)(void)) :
	cDevice(),
	m_onPrimaryDevice(onPrimaryDevice),
	m_audioId(0),
	m_trickMode(false),
	m_scheduler()
{
	memset(&m_videoCodec, 0, sizeof(codec_para_t));
	memset(&m_audioCodec, 0, sizeof(codec_para_t));

	m_videoCodec.stream_type = STREAM_TYPE_ES_VIDEO;
	m_audioCodec.stream_type = STREAM_TYPE_ES_AUDIO;

	codec_audio_basic_init();
}

cAmlDevice::~cAmlDevice()
{
	DeInit();
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
	Width = 1920;
	Height = 1080;
	PixelAspect = (double)Width / Height;
}

int64_t cAmlDevice::GetSTC(void)
{
	return m_trickMode ? m_scheduler.GetPts() : (m_audioCodec.has_audio ?
			codec_get_apts(&m_audioCodec) : codec_get_vpts(&m_videoCodec));
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
		cSysFs::Write("/sys/class/video/blackout_policy", 1);
		Clear();
		break;

	case pmAudioVideo:
	case pmAudioOnly:
	case pmAudioOnlyBlack:
	case pmVideoOnly:
		cSysFs::Write("/sys/class/video/blackout_policy", 0);
		break;

	default:
		break;
	}

	return true;
}

int cAmlDevice::PlayVideo(const uchar *Data, int Length)
{
	int ret = Length;
	int payloadLen = Length - PesPayloadOffset(Data);
	const uchar *payload = Data + PesPayloadOffset(Data);

	if (!m_videoCodec.has_video)
	{
		int streamType = PatPmtParser()->Vtype();

		m_videoCodec.has_video = 1;
		m_videoCodec.am_sysinfo.param = m_trickMode ? 0 : (void *)(EXTERNAL_PTS);
		m_videoCodec.video_type =
				streamType == 0x01 ? VFORMAT_MPEG12 :
				streamType == 0x02 ? VFORMAT_MPEG12 :
				streamType == 0x1b ? VFORMAT_H264   : VFORMAT_UNKNOWN;

		m_videoCodec.am_sysinfo.format =
				m_videoCodec.video_type == VFORMAT_H264 ?
						VIDEO_DEC_FORMAT_H264 : VIDEO_DEC_FORMAT_UNKNOW;

		if (codec_init(&m_videoCodec) != CODEC_ERROR_NONE)
		{
			ELOG("failed to init video codec!");
			m_videoCodec.has_video = 0;
		}
		else
		{
			DLOG("set video codec to: %s",
					m_videoCodec.video_type == VFORMAT_MPEG12 ? "MPEG2" :
					m_videoCodec.video_type == VFORMAT_H264 ? "H264" : "UNKNOWN");

			if (!m_trickMode)
			{
				codec_set_cntl_avthresh(&m_videoCodec, AV_SYNC_THRESH);
				codec_set_cntl_syncthresh(&m_videoCodec, 0);

				cSysFs::Write("/sys/class/tsync/mode", m_audioCodec.has_audio ? 1 : 0);
				cSysFs::Write("/sys/class/tsync/enable", 1);
				cSysFs::Write("/sys/class/tsync/pts_pcrscr", "0x0");
			}
		}
	}
	if (m_videoCodec.has_video)
	{
		uint64_t pts = PesHasPts(Data) ? PesGetPts(Data) : INVALID_PTS;
		if (m_scheduler.Check(pts))
		{
			if (!m_trickMode)
				codec_checkin_pts(&m_videoCodec, pts);
		}
		else
			ret = 0;

		while (ret && m_videoCodec.has_video && payloadLen > 0)
		{
			int written = codec_write(&m_videoCodec, (void *)payload, payloadLen);
			if (written > 0)
			{
				payloadLen -= written;
				payload += written;
			}
			else
				ret = 0;
		}
	}
	return ret;
}

int cAmlDevice::PlayAudio(const uchar *Data, int Length, uchar Id)
{
	int ret = Length;
	const uchar *payload = Data + PesPayloadOffset(Data);
	int payloadLen = Length - PesPayloadOffset(Data);

	if (!m_audioCodec.has_audio || m_audioId != Id)
	{
		bool reset = m_audioCodec.has_audio > 0;

		m_audioId = Id;
		m_audioCodec.has_audio = 1;
		m_audioCodec.am_sysinfo.param = (void *)(EXTERNAL_PTS);
		m_audioCodec.audio_channels = 0;
		m_audioCodec.audio_samplerate = 0;
		m_audioCodec.audio_info.channels = 0;
		m_audioCodec.audio_info.sample_rate = 0;
		m_audioCodec.audio_type = AFORMAT_UNKNOWN;

		if (Data[3] >= 0xc0 && Data[3] <= 0xdf)
			m_audioCodec.audio_type = AFORMAT_MPEG;
		else
		{
			if ((payload[0] & 0xf8) == 0x88)
				m_audioCodec.audio_type = AFORMAT_DTS;
			else if ((payload[0] & 0xf8) == 0x80)
				m_audioCodec.audio_type = AFORMAT_AC3;
		}

		if ((reset ? codec_reset_audio(&m_audioCodec) :
				codec_init(&m_audioCodec)) != CODEC_ERROR_NONE)
		{
			ELOG("failed to init audio codec!");
			m_audioCodec.has_audio = 0;
		}
		else
		{
			DLOG("set audio codec to: %s",
					m_audioCodec.audio_type == AFORMAT_MPEG ? "MPEG" :
					m_audioCodec.audio_type == AFORMAT_AC3 ? "AC-3" :
					m_audioCodec.audio_type == AFORMAT_DTS ? "DTS" : "UNKNOWN");

			if (m_trickMode)
				codec_set_cntl_mode(&m_videoCodec, TRICKMODE_I);
			else
			{
				cSysFs::Write("/sys/class/tsync/mode", 1);
				cSysFs::Write("/sys/class/tsync/enable", 1);
				cSysFs::Write("/sys/class/tsync/pts_pcrscr", "0x0");
			}
		}
	}
	if (m_audioCodec.has_audio)
	{
		uint64_t pts = PesHasPts(Data) ? PesGetPts(Data) : INVALID_PTS;
		if (m_scheduler.Check(pts))
		{
			if (!m_trickMode)
				codec_checkin_pts(&m_audioCodec, pts);
		}
		else
			ret = 0;

		while (ret && m_audioCodec.has_audio && payloadLen > 0)
		{
			int written = codec_write(&m_audioCodec, (void *)payload, payloadLen);
			if (written > 0)
			{
				payloadLen -= written;
				payload += written;
			}
			else
				ret = 0;
		}
	}
	return ret;
}

void cAmlDevice::StillPicture(const uchar *Data, int Length)
{
	if (Data[0] == 0x47)
		cDevice::StillPicture(Data, Length);
	else
	{
		DBG("%s", __FUNCTION__);
		if (!m_videoCodec.has_video)
		{
			int streamType = PatPmtParser()->Vtype();

			m_videoCodec.has_video = 1;
			m_videoCodec.am_sysinfo.param = 0;//(void *)(EXTERNAL_PTS);
			m_videoCodec.video_type =
					streamType == 0x01 ? VFORMAT_MPEG12 :
					streamType == 0x02 ? VFORMAT_MPEG12 :
					streamType == 0x1b ? VFORMAT_H264   : VFORMAT_UNKNOWN;

			m_videoCodec.am_sysinfo.format =
					m_videoCodec.video_type == VFORMAT_H264 ?
							VIDEO_DEC_FORMAT_H264 : VIDEO_DEC_FORMAT_UNKNOW;

			if (codec_init(&m_videoCodec) != CODEC_ERROR_NONE)
			{
				ELOG("failed to init video codec!");
				m_videoCodec.has_video = 0;
			}
			else
			{
				codec_set_cntl_mode(&m_videoCodec, TRICKMODE_I);
				m_trickMode = true;
			}
		}
		if (m_videoCodec.has_video)
		{
			int repeat = m_videoCodec.video_type == VFORMAT_H264 ? 12 : 4;
			while (repeat--)
			{
				int length = Length;
				const uchar *data = Data;

				while (PesLongEnough(length))
				{
					int pktLen = PesHasLength(data) ? PesLength(data) : length;

					if ((data[3] & 0xf0) == 0xe0)
						PlayVideo(data, pktLen);

					data += pktLen;
					length -= pktLen;
				}
			}
		}
	}
}

void cAmlDevice::Clear(void)
{
	DBG("%s", __FUNCTION__);

	cSysFs::Write("/sys/class/tsync/enable", 0);
	cSysFs::Write("/sys/class/tsync/mode", 0);

	if (m_audioCodec.has_audio)
		codec_close(&m_audioCodec);

	if (m_videoCodec.has_video)
	{
		if (m_trickMode)
		{
			m_trickMode = false;
			codec_resume(&m_videoCodec);
			codec_set_cntl_mode(&m_videoCodec, TRICKMODE_NONE);
		}
		codec_close(&m_videoCodec);
	}
	m_audioCodec.has_audio = 0;
	m_videoCodec.has_video = 0;

	m_scheduler.Reset();
}

void cAmlDevice::Play(void)
{
	DBG("%s", __FUNCTION__);

	if (m_videoCodec.has_video)
	{
		codec_resume(&m_videoCodec);
		codec_set_cntl_mode(&m_videoCodec, TRICKMODE_NONE);
	}
	if (m_audioCodec.has_audio)
		codec_resume(&m_audioCodec);
}

void cAmlDevice::Freeze(void)
{
	DBG("%s", __FUNCTION__);

	if (m_videoCodec.has_video)
		codec_pause(&m_videoCodec);

	if (m_audioCodec.has_audio)
		codec_pause(&m_audioCodec);
}

void cAmlDevice::TrickSpeed(int Speed, bool Forward)
{
	DBG("%s", __FUNCTION__);
	m_trickMode = true;
	m_scheduler.SetSpeed(Speed, Forward);
}

bool cAmlDevice::Poll(cPoller &Poller, int TimeoutMs)
{
	if (m_videoCodec.has_video)
		Poller.Add(m_videoCodec.handle, true);

	if (m_audioCodec.has_audio)
		Poller.Add(m_audioCodec.handle, true);

	return Poller.Poll(TimeoutMs);
}

bool cAmlDevice::Flush(int TimeoutMs)
{
	DBG("%s", __FUNCTION__);
	return true;
}

void cAmlDevice::SetVolumeDevice(int Volume)
{
	DBG("%s(%d)", __FUNCTION__, Volume);
}

/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "fbosd.h"
#include "tools.h"

/* ------------------------------------------------------------------------- */

class cFbRawOsd : public cOsd
{
public:

	cFbRawOsd(int Left, int Top, uint Level, const char* device) :
		cOsd(Left, Top, Level),
		m_fd(0),
		m_fb(0),
		m_size(0)
	{
		if (OpenFrameBuffer(device))
			DLOG("opened OSD frame buffer %dx%d, %dbpp",
					m_vinfo.xres, m_vinfo.yres, m_vinfo.bits_per_pixel);
	}

	virtual ~cFbRawOsd()
	{
		SetActive(false);

		if (m_fb)
			munmap(m_fb, m_size);

		if (m_fd)
			close(m_fd);
	}

	virtual void Flush(void)
	{
		if (!Active() || !m_fb)
			return;

		if (IsTrueColor())
		{
			LOCK_PIXMAPS;
			while (cPixmapMemory *pm =
					dynamic_cast<cPixmapMemory *>(RenderPixmaps()))
			{
				const uint8_t *src = pm->Data();
				char *dst = m_fb + (Left() + pm->ViewPort().Left()) *
						(m_vinfo.bits_per_pixel / 8 ) +
						(Top() + pm->ViewPort().Top()) * m_finfo.line_length;

				for (int y = 0; y < pm->DrawPort().Height(); y++)
				{
					memcpy(dst, src, pm->DrawPort().Width() * sizeof(tColor));
					src += pm->DrawPort().Width() * sizeof(tColor);
					dst += m_finfo.line_length;
				}
#if APIVERSNUM >= 20110
				DestroyPixmap(pm);
#else
				delete pm;
#endif
			}
		}
		else
		{
			for (int i = 0; cBitmap *bitmap = GetBitmap(i); ++i)
			{
				int x1, y1, x2, y2;
				if (bitmap->Dirty(x1, y1, x2, y2))
				{
					char *dst = m_fb + (Left() + bitmap->X0() + x1) *
							(m_vinfo.bits_per_pixel / 8 ) +
							(Top() + bitmap->Y0() + y1) * m_finfo.line_length;

					for (int y = y1; y <= y2; ++y)
					{
						tColor *p = (tColor *)dst;
						for (int x = x1; x <= x2; ++x)
							*p++ = bitmap->GetColor(x, y);

						dst += m_finfo.line_length;
					}
					bitmap->Clean();
				}
			}
		}
	}

	virtual eOsdError SetAreas(const tArea *Areas, int NumAreas)
	{
		eOsdError error;
		cBitmap *bitmap;

		if (Active())
			Clear();

		error = cOsd::SetAreas(Areas, NumAreas);

		for (int i = 0; (bitmap = GetBitmap(i)) != NULL; i++)
			bitmap->Clean();

		return error;
	}

protected:

	virtual void SetActive(bool On)
	{
		if (On != Active())
		{
			cOsd::SetActive(On);
			if (!On)
				Clear();
			else
				if (GetBitmap(0))
					Flush();
		}
	}

	virtual void Clear(void)
	{
		if (m_fb)
			memset(m_fb, 0, m_size);
	}

private:

	bool OpenFrameBuffer(const char* device)
	{
		m_fd = open(device, O_RDWR);
		if (m_fd == -1)
		{
			ELOG("failed to open frame buffer device!");
			return false;
		}

		if (ioctl(m_fd, FBIOGET_FSCREENINFO, &m_finfo) == -1)
		{
			ELOG("failed to get fixed frame buffer info!");
			return false;
		}

		if (ioctl(m_fd, FBIOGET_VSCREENINFO, &m_vinfo) == -1)
		{
			ELOG("failed to get variable frame buffer info!");
			return false;
		}

		if (m_vinfo.bits_per_pixel != 32)
		{
			ELOG("wrong frame buffer pixel format, only ARGB supported!");
			close(m_fd);
			m_fd = 0;
			return false;
		}

		m_size = m_vinfo.xres * m_vinfo.yres * m_vinfo.bits_per_pixel / 8;
		m_fb = (char *)mmap(0, m_size, PROT_READ | PROT_WRITE, MAP_SHARED,
				m_fd, 0);

		if (m_fd == -1)
		{
			ELOG("failed to map frame buffer device to memory!");
			return false;
		}
		return true;
	}

	int m_fd;
	char *m_fb;
	long int m_size;

	fb_var_screeninfo m_vinfo;
	fb_fix_screeninfo m_finfo;
};

/* ------------------------------------------------------------------------- */

cFbOsdProvider::cFbOsdProvider(const char* device) :
	cOsdProvider(),
	m_device(device)
{
	DLOG("new cOsdProvider()");
}

cFbOsdProvider::~cFbOsdProvider()
{
	DLOG("delete cOsdProvider()");
}

cOsd *cFbOsdProvider::CreateOsd(int Left, int Top, uint Level)
{
	return new cFbRawOsd(Left, Top, Level, m_device);
}

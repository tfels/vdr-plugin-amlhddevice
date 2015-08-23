/*
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "tools.h"

bool cSysFs::Write(const char *path, const char *value)
{
	int fd = open(path, O_RDWR, 0644);
    if (fd >= 0)
    {
    	int ret = write(fd, value, strlen(value));
    	if (ret < 0)
    		ELOG("failed to write %s to %s (-%d)", value, path, errno);
        close(fd);
        return true;
    }
	return false;
}

bool cSysFs::Write(const char *path, int value)
{
    int fd = open(path, O_RDWR, 0644);
    if (fd >= 0)
    {
    	char cmd[64];
    	snprintf(cmd, sizeof(cmd), "%d", value);

    	int ret = write(fd, cmd, strlen(cmd));
    	if (ret < 0)
    		ELOG("failed to write %d to %s (-%d)", value, path, errno);
        close(fd);
        return true;
    }
	return false;
}

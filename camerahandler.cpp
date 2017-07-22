#include "camerahandler.h"
#include <QDebug>
#include <QFileDevice>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

Q_LOGGING_CATEGORY(camera, "camera.handler")
#define LOG_ERROR qCCritical(camera)
#define LOG_DEBUG qCDebug(camera)
#define LOG_WARN qCWarning(camera)

bool checkPath (const char* path) {
    struct stat64 st;
    if (stat64(path, &st) == -1) {
        qCCritical(camera) << "Cannot stat" << path << ":" << strerror(errno);
        return false;
    }

    if (!S_ISCHR(st.st_mode)) {
        LOG_ERROR << "Not a character device" << path;
        return false;
    }
    return true;
}


static int xioctl (int fh, int request, void *arg) {
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    LOG_ERROR << "ioctl: " << strerror(errno);
    return r;
}


CameraHandler::CameraHandler(const char *path) {
    if (checkPath(path))
        m_handler = open(path, O_RDWR | O_NONBLOCK, 0);
    if (m_handler == -1) {
        m_handler = 0;
        LOG_ERROR << "Cannot open" << path << "|" << errno << strerror(errno);
    }
}

bool CameraHandler::initDevice() {
    if (!m_handler)
        return false;

//    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    int res = 0;

    res = xioctl(m_handler, VIDIOC_QUERYCAP, &m_capability);
    if (res == -1) {
        LOG_ERROR << "Cannot query capabilities. Not V4L device? :" << strerror(errno);
        return false;
    }

    if (!(m_capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOG_WARN << "Device does not support capture!";
        return false;
    }

    if (!(m_capability.capabilities & V4L2_CAP_STREAMING)) {
        LOG_WARN << "No streaming capability!";
    }

    if (!(m_capability.capabilities & V4L2_CAP_READWRITE)) {
        LOG_WARN << "Device does not support read/write syscall!";
    }

}

v4l2_format CameraHandler::getFormat_() {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    xioctl(m_handler, VIDIOC_G_FMT, &fmt);
    return fmt;
}

bool CameraHandler::setFormat_(const v4l2_format *fmt) {
    return xioctl(m_handler, VIDIOC_S_FMT, const_cast<v4l2_format*>(fmt)) == 0;
}

QSize CameraHandler::size() const {
    return m_size;
}

void CameraHandler::setSize(const QSize &size) {
    m_size = size;
}

Format CameraHandler::format() const {
    return m_format;
}

void CameraHandler::setFormat(const Format &format) {
    m_format = format;
}



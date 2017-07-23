#include "camerahandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <QSize>
#include <iostream>


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

static QImage::Format convertFormat (Format f) {
    switch (f) {
    case Format::BGR:
        return QImage::Format_BGR30;
    case Format::RGB:
        return QImage::Format_RGB888;
    case Format::Grey:
        return QImage::Format_Grayscale8;
    default:
        return QImage::Format_Invalid;
    }
}

static int xioctl (int fh, int request, void *arg) {
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    if (r == -1)
        LOG_ERROR << "ioctl: " << strerror(errno);
    return r;
}

static void unmapAll (QVector<Buffer>& buffers) {
    for (auto& buf : buffers) {
        munmap(buf.address, buf.length);
    }
}

CameraHandler::CameraHandler(const char *path) {
    memset(&m_capability, 0, sizeof(m_capability));
    if (checkPath(path))
        m_handler = open(path, O_RDWR | O_NONBLOCK, 0);
    if (m_handler == -1) {
        CLEAR(m_capability);
        m_handler = 0;
        LOG_ERROR << "Cannot open" << path << "|" << errno << strerror(errno);
    }
    if (!initDevice()) {
        m_handler = 0;
    }
}

bool CameraHandler::initDevice() {
    if (!m_handler)
        return false;

    int res = 0;
    res = xioctl(m_handler, VIDIOC_QUERYCAP, &m_capability);
    if (res == -1) {
        LOG_ERROR << "Cannot query capabilities. Not V4L device? :" << strerror(errno);
        return false;
    }

    if (!isCapture()) {
        LOG_WARN << "Device does not support capture!";
    }
    if (!isStreaming()) {
        LOG_WARN << "No streaming capability!";
    }
    if (!isReadWrite()) {
        LOG_WARN << "Device does not support read/write syscall!";
    }
//    setCropping(QRect());
//    auto fmt = getFormat_();
//    setFormat_(&fmt);
    initBuffer(requestBuffers(10).count);
    startStream();
    return true;
}

v4l2_requestbuffers CameraHandler::requestBuffers(int count) {
    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.count = count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(m_handler, VIDIOC_REQBUFS, &req) == -1) {
        if (errno == EINVAL)
            LOG_ERROR << "Device does not support memory mapping";
        else
            LOG_ERROR << "Cannot request buffers:" << strerror(errno);
        CLEAR(req);
    }
    if (req.count < 2) {
        LOG_ERROR << "Insufficient beffer memory!";
        CLEAR(req);
    }
    return req;
}

v4l2_format CameraHandler::getFormat_() {
    struct v4l2_format fmt;
    CLEAR(fmt);
    if (xioctl(m_handler, VIDIOC_G_FMT, &fmt) == -1) {
        LOG_ERROR << "GetFormat:" << strerror(errno);
    }
    return fmt;
}

VideoFrame CameraHandler::readFrame() {
    struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
//    while (true) {
//        fd_set fds;
//        struct timeval tv;
//        int r;

//        FD_ZERO(&fds);
//        FD_SET(m_handler, &fds);

//        /* Timeout. */
//        tv.tv_sec = 2;
//        tv.tv_usec = 0;

//        r = select(m_handler + 1, &fds, NULL, NULL, &tv);

//        if (-1 == r) {
//            if (EINTR == errno)
//                continue;
//            LOG_ERROR << "Processing loop:" << strerror(errno);
//            break;
//        }
//        if (r == 0) {
//            LOG_ERROR << "select timeout";
//        }
//    }

    int res = xioctl(m_handler, VIDIOC_DQBUF, &buf);
    while (res) {
        if (res == -1 && errno != EAGAIN) {
            LOG_ERROR << "readFrame error:" << strerror(errno);
//            break;
        }
        res = xioctl(m_handler, VIDIOC_DQBUF, &buf);
    }

    assert(buf.index < (quint64)m_buffers.size());

    return VideoFrame(this, buf);
}

v4l2_buffer CameraHandler::checkFrame() {
    struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(m_handler, VIDIOC_DQBUF, &buf) == -1) {
        switch (errno) {
        case EAGAIN:
            CLEAR(buf);
            return buf;
        case EIO:
            /* fall through */
        default:
            LOG_WARN << "Get image error:" << strerror(errno);
            CLEAR(buf);
            return buf;
        }
    }

    assert(buf.index < (quint64) m_buffers.size());
    return buf;
}

void CameraHandler::releaseBuffer(const v4l2_buffer& buf) {
    if (-1 == xioctl(m_handler, VIDIOC_QBUF, const_cast<v4l2_buffer*>(&buf)))
        LOG_ERROR << "Failed to return buffer to queue! Index:" << buf.index;
}

void CameraHandler::processingLoop()
{
    for (;;) {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO(&fds);
        FD_SET(m_handler, &fds);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(m_handler + 1, &fds, NULL, NULL, &tv);

        if (-1 == r) {
            if (EINTR == errno)
                continue;
            LOG_ERROR << "Processing loop:" << strerror(errno);
            break;
        }

        if (0 == r) {
            LOG_ERROR << "select() timeout!";
            break;
        }

        auto buf = checkFrame();
        if (!buf.bytesused)
            break;
        /* EAGAIN - continue select loop. */
    }
}

v4l2_format CameraHandler::setFormat_ (v4l2_format* format) {
    v4l2_format fmt;
    if (format) {
        fmt = *format;
    } else {
        CLEAR(fmt);
    }

    fmt.fmt.pix.width = m_size.width();
    fmt.fmt.pix.height = m_size.height();
    fmt.fmt.pix.pixelformat = static_cast<quint32>(m_pixelFormat);
    fmt.fmt.pix.colorspace = static_cast<quint32>(m_colorspace);
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    xioctl(m_handler, VIDIOC_S_FMT, &fmt);
    return fmt;
}

QVector<Buffer> CameraHandler::initBuffer (int count) {
    auto req = requestBuffers(count);

    QVector<Buffer> buffers;
    buffers.resize(count);
    for (quint32 i = 0; i < req.count; ++i) {
        struct v4l2_buffer buff;
        CLEAR(buff);
        buff.type = req.type;
        buff.memory = V4L2_MEMORY_MMAP;
        buff.index = i;

        if (xioctl(m_handler, VIDIOC_QUERYBUF, &buff) == -1) {
            LOG_ERROR << "Cannot query memory from device!";
        }
        buffers[i] = { mmap(nullptr, buff.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                           m_handler, buff.m.offset),
                           buff.length };
        if (buffers[i].address == MAP_FAILED) {
            LOG_ERROR << "mmap failed!";
            unmapAll(buffers);
            buffers.clear();
            break;
        }
    }
    return buffers;
}

bool CameraHandler::startStream() {
    for (int i = 0; i < m_buffers.size(); ++i) {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(m_handler, VIDIOC_QBUF, &buf) == -1) {
//            close();
            return false;
        }
    }
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return xioctl(m_handler, VIDIOC_STREAMON, &type) != -1;
}

bool CameraHandler::stopStream() {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return xioctl(m_handler, VIDIOC_STREAMOFF, &type) != -1;
}

bool CameraHandler::setCropping(const QRect& rect) {
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    CLEAR(cropcap);
    CLEAR(crop);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(m_handler, VIDIOC_CROPCAP, &cropcap) == 0) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c.top = rect.top();
        crop.c.left = rect.left();
        crop.c.width = rect.width();
        crop.c.height = rect.height();
        if (rect.isEmpty())
            crop.c = cropcap.defrect;

        if (-1 == xioctl(m_handler, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
            case EINVAL:
            default:
                LOG_WARN << "Cropping is not supported!";
                return false;
            }
        }
    } else {
        LOG_WARN << "setCropping error:" << strerror(errno);
        return false;
    }
    return true;
}

QSize CameraHandler::size() const {
    return m_size;
}

void CameraHandler::setSize(const QSize &size) {
    m_size = size;
}

void CameraHandler::close() {
    unmapAll(m_buffers);
    m_buffers.clear();
    ::close(m_handler);
    m_handler = 0;
    CLEAR(m_capability);
    CLEAR(m_format);
}

CameraHandler::~CameraHandler() {
    close();
}

Format CameraHandler::format() const {
    return m_pixelFormat;
}

void CameraHandler::setFormat(const Format &format) {
    m_pixelFormat = format;
}



QImage VideoFrame::toImage() const {
    return QImage((const uchar*) buffer().address, m_handler->size().width(),
                  m_handler->size().height(), convertFormat(m_handler->format()));
}

//QPixmap VideoFrame::toPixmap() const
//{
//    return QPixmap()
//}

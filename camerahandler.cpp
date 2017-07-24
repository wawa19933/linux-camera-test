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
#include <QMap>

static QMap<quint32, QString> options = {
    { VIDIOC_QUERYCAP, "Quering capabilites" },
    { VIDIOC_CREATE_BUFS, "Creating buffers" },
    { VIDIOC_CROPCAP, "Getting crop capabilites" },
    { VIDIOC_DQBUF, "Taking buffer from driver queue" },
    { VIDIOC_QBUF, "Releasing buffer to driver queue" },
    { VIDIOC_QUERYBUF, "Requesting buffers" },
    { VIDIOC_STREAMON, "Starting stream" },
    { VIDIOC_STREAMOFF, "Stopping stream" },
    { VIDIOC_S_CROP, "Setting crop" },
    { VIDIOC_G_CROP, "Getting crop" },
    { VIDIOC_S_FMT, "Setting format" },
    { VIDIOC_G_FMT, "Getting format" }
};

//static QMap<PixelFormat,QImage::Format> imageFormats = {
//    { PixelFormat::RGB, QImage::FOR
//};

Q_LOGGING_CATEGORY(camera, "camera.handler")
#define LOG_ERROR qCCritical(camera)
#define LOG_DEBUG qCDebug(camera)
#define LOG_WARN qCWarning(camera)


static QImage::Format convertFormat (PixelFormat f) {
    switch (f) {
    case PixelFormat::BGR:
        return QImage::Format_BGR30;
    case PixelFormat::RGB:
        return QImage::Format_RGB888;
    case PixelFormat::Grey:
        return QImage::Format_Grayscale8;
    default:
        return QImage::Format_Invalid;
    }
}

CameraHandler::CameraHandler(const char *path) {
    CLEAR(m_capability);
    if (!(openDevice(path) && initialize())) {
        close();
    }
}

QImage CameraHandler::getFrame() {
    auto buf = takeBuffer();
    QImage img;
    if (buf.bytesused == 0) {
        return img;
    }
    img.loadFromData(reinterpret_cast<const uchar*>(m_buffers[static_cast<int>(buf.index)].address), buf.bytesused, "jpeg");
    releaseBuffer(buf.index);
    return img;
//    return QImage(reinterpret_cast<const uchar*>(m_buffers[static_cast<int>(buf.index)].address),
//            m_frameSize.width(), m_frameSize.height(), QImage::Format_RGB888);
}

bool CameraHandler::query (quint32 request, void *arg) const {
    int res;
    do {
        res = ioctl(m_handler, request, arg);
    } while (res == -1 && errno == EINTR);

    if (res == -1) {
        if (options.contains(request))
            LOG_ERROR << options[request] << ":" << strerror(errno);
        else
            LOG_ERROR << "ioctl: " << strerror(errno);
    }
    return res == 0;
}

bool CameraHandler::openDevice(const char *path) {
    struct stat64 st;
    if (stat64(path, &st) == -1) {
        LOG_ERROR << "Cannot stat" << path << ":" << strerror(errno);
        return false;
    }
    if (!S_ISCHR(st.st_mode)) {
        LOG_ERROR << "Not a character device" << path;
        return false;
    }
    // Open device
    m_handler = open(path, O_RDWR | O_NONBLOCK, 0);
    if (m_handler == -1) {
        m_handler = 0;
        LOG_ERROR << "Cannot open" << path << "|" << errno << strerror(errno);
        return false;
    }
    return true;
}

bool CameraHandler::initialize() {
    if (!getCapabilities())
        return false;
    if (!isCapture())
        LOG_WARN << "Device does not support capture!";
    if (!isStreaming())
        LOG_WARN << "No streaming capability!";
// Cropping
//    if (!setCropping(defaultRect()));
//    m_crop = defaultRect();
// Format
    auto fmt = getFmt();
    m_colorspace = static_cast<Colorspace>(fmt.fmt.pix.colorspace);
    m_pixelFormat = static_cast<PixelFormat>(fmt.fmt.pix.pixelformat);
    m_frameSize.setWidth(fmt.fmt.pix.width);
    m_frameSize.setHeight(fmt.fmt.pix.height);
    // Initialize buffer
    if (!initBuffers(10))
        return false;
    if (!enableStreaming())
        return false;
    return true;
}

bool CameraHandler::initBuffers (quint32 count) {
    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.count = count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (!query(VIDIOC_REQBUFS, &req))
        return false;

    m_buffers.resize(static_cast<int>(req.count));
    struct v4l2_buffer buf;
    CLEAR(buf);
    for (qint32 i = 0; i < m_buffers.size(); ++i) {
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.index = i;
        if (!query(VIDIOC_QUERYBUF, &buf))
            return false;

        m_buffers[i].length = buf.length;
        m_buffers[i].address = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                     m_handler, buf.m.offset);

        if (m_buffers[i].address == MAP_FAILED) {
            LOG_ERROR << "mmap() failed!";
            m_buffers.clear();
            return false;
        }
    }
    return true;
}

bool CameraHandler::getCapabilities() {
    return query(VIDIOC_QUERYCAP, &m_capability);
}

bool CameraHandler::enableStreaming() {
    for (int i = 0; i < m_buffers.size(); ++i)
        releaseBuffer(i);

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return query(VIDIOC_STREAMON, &type);
}

bool CameraHandler::disableStreaming() {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    return query(VIDIOC_STREAMOFF, &type);
}

bool CameraHandler::releaseBuffer(int index) {
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    return query(VIDIOC_QBUF, &buf);
}

v4l2_buffer CameraHandler::takeBuffer() {
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    //TODO: EAGAIN handling
    if (!query(VIDIOC_DQBUF, &buf))
        CLEAR(buf);
    return buf;
}

void CameraHandler::unmapBuffers() {
    for (auto& buffer : m_buffers) {
        if (munmap(buffer.address, buffer.length) == -1)
            LOG_ERROR << "Memory unmap failed:" << strerror(errno);
    }
}

void CameraHandler::applySettings() {
    auto fmt = getFmt();
    fmt.fmt.pix.pixelformat = static_cast<quint32>(m_pixelFormat);
    fmt.fmt.pix.width = m_frameSize.width();
    fmt.fmt.pix.height = m_frameSize.height();
    setFmt(&fmt);
    if (fmt.fmt.pix.width != static_cast<quint32>(m_frameSize.width())
            || fmt.fmt.pix.height != static_cast<quint32>(m_frameSize.height())) {
        LOG_WARN << "Cannot set frame size";
        m_frameSize.setWidth(fmt.fmt.pix.width);
        m_frameSize.setHeight(fmt.fmt.pix.height);
    }
    if (fmt.fmt.pix.colorspace != static_cast<quint32>(m_colorspace)) {
        LOG_WARN << "Cannot set colorspace";
        m_colorspace = static_cast<Colorspace>(fmt.fmt.pix.colorspace);
    }
    if (fmt.fmt.pix.pixelformat != static_cast<quint32>(m_pixelFormat)) {
        LOG_WARN << "Cannot set pixel format";
        m_pixelFormat = static_cast<PixelFormat>(fmt.fmt.pix.pixelformat);
    }
}

v4l2_format CameraHandler::getFmt () {
    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!query(VIDIOC_G_FMT, &fmt))
        CLEAR(fmt);
    return fmt;
}

v4l2_format CameraHandler::setFmt (v4l2_format* fmt) {
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    query(VIDIOC_S_FMT, fmt);
    return *fmt;
}

bool CameraHandler::setCropping(const QRect& rect) {
    struct v4l2_crop crop;
    CLEAR(crop);
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c.top = rect.top();
    crop.c.left = rect.left();
    crop.c.width = rect.width();
    crop.c.height = rect.height();

    if (!query(VIDIOC_S_CROP, &crop)) {
        if (errno == EINVAL)
            LOG_WARN << "Cropping is not supported!";
        return false;
    }
    return true;
}

QSize CameraHandler::size() const {
    return m_frameSize;
}

void CameraHandler::setSize(const QSize &size) {
    auto fmt = getFmt();
    fmt.fmt.pix.width = size.width();
    fmt.fmt.pix.height = size.height();
    disableStreaming();
    setFmt(&fmt);
    enableStreaming();
    if (fmt.fmt.pix.width != size.width() || fmt.fmt.pix.height != size.height())
        LOG_WARN << "Cannot change frame size";
    else
        m_frameSize = size;
}

void CameraHandler::close() {
    unmapBuffers();
    m_buffers.clear();
    ::close(m_handler);
    m_handler = 0;
    CLEAR(m_capability);
}

CameraHandler::~CameraHandler() {
    close();
}

PixelFormat CameraHandler::format() const {
    return m_pixelFormat;
}

void CameraHandler::setFormat(const PixelFormat &format) {
    auto fmt = getFmt();
    fmt.fmt.pix.pixelformat = static_cast<quint32>(m_pixelFormat);
    setFmt(&fmt);
    if (fmt.fmt.pix.pixelformat == static_cast<quint32>(m_pixelFormat))
        m_pixelFormat = format;
    else
        LOG_WARN << "Cannot change pixel format";
}

QRect CameraHandler::defaultRect() const {
    struct v4l2_cropcap cropcap;
    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!query(VIDIOC_CROPCAP, &cropcap)) {
        return QRect();
    }
    return QRect(cropcap.defrect.left, cropcap.defrect.top, cropcap.defrect.width, cropcap.defrect.height);
}

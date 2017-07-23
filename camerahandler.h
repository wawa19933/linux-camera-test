#ifndef CAMERAHANDLER_H
#define CAMERAHANDLER_H

#include <QString>
#include <QLoggingCategory>
#include <QSize>
#include <QRect>
#include <QPixmap>
#include <QImage>

#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <future>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
Q_DECLARE_LOGGING_CATEGORY(camera)


struct Buffer {
    void* address {};
    size_t length {};
};

class VideoFrame;
class FrameBuffer;

enum class Format { RGB = V4L2_PIX_FMT_RGB24, BGR = V4L2_PIX_FMT_BGR24,
                    Grey = V4L2_PIX_FMT_GREY };
enum class Colorspace { Default = V4L2_COLORSPACE_DEFAULT, sRGB = V4L2_COLORSPACE_SRGB,
                       AdobeRGB = V4L2_COLORSPACE_ADOBERGB, JPEG = V4L2_COLORSPACE_JPEG,
                       RAW = V4L2_COLORSPACE_RAW };


class CameraHandler
{
public:
    CameraHandler (const char* path = "/dev/video0");

    bool isCapture () const { return (m_capability.capabilities & V4L2_CAP_VIDEO_CAPTURE); }
    bool isMemToMem () const { return (m_capability.capabilities & V4L2_CAP_VIDEO_M2M); }
    bool isReadWrite () const { return (m_capability.capabilities & V4L2_CAP_READWRITE); }
    bool isAsync () const { return (m_capability.capabilities & V4L2_CAP_ASYNCIO); }
    bool isStreaming () const { return (m_capability.capabilities & V4L2_CAP_STREAMING); }
    bool hasOverlay () const { return (m_capability.capabilities & V4L2_CAP_VIDEO_OVERLAY); }
    bool hasControls () const { return (m_capability.capabilities & V4L2_CAP_DEVICE_CAPS); }
    bool hasExtendedPixelFormat () const { return (m_capability.capabilities & V4L2_CAP_EXT_PIX_FORMAT); }
    bool hasCropping () const {
        struct v4l2_crop crop;
        return (ioctl(m_handler, VIDIOC_S_CROP, &crop) != -1);
    }
    QString deviceName () const { return (const char*)m_capability.card; }
    QString driverName () const { return reinterpret_cast<const char*>(m_capability.driver); }
    QString deviceBus () const { return reinterpret_cast<const char*>(m_capability.bus_info); }

    Format format() const;
    void setFormat(const Format &format);

    QSize size() const;
    void setSize(const QSize &size);
    VideoFrame readFrame();
    void close ();
    ~CameraHandler ();

private:
    bool initDevice ();
    v4l2_requestbuffers requestBuffers (int count);
    QVector<Buffer> initBuffer (int count);
    bool setCropping (const QRect& rect);
    v4l2_format getFormat_ ();
    v4l2_format setFormat_ (v4l2_format* format = nullptr);
    bool startStream ();
    bool stopStream ();
    v4l2_buffer checkFrame ();
    void releaseBuffer (const v4l2_buffer& buf);
    void processingLoop ();

private:
    struct v4l2_format m_format;
    struct v4l2_capability m_capability;
    QVector<Buffer> m_buffers;
    std::promise<void> m_promise;
    QSize m_size { 640, 480 };
    int m_handler {};
    Format m_pixelFormat { Format::RGB };
    Colorspace m_colorspace { Colorspace::RAW };

    friend class VideoFrame;
};


class VideoFrame {
public:
    VideoFrame (CameraHandler* handler, v4l2_buffer buf) : m_buffer(buf), m_handler(handler) {}

    Buffer& buffer () { return m_handler->m_buffers[m_buffer.index]; }

    const Buffer& buffer () const { return m_handler->m_buffers[m_buffer.index]; }
    const char* data () const { return (char*) buffer().address; }
    quint64 size () const { return m_buffer.bytesused; }
    quint64 length () const { return m_buffer.bytesused; }

    QImage toImage () const;
//    QPixmap toPixmap () const;

    ~VideoFrame () {
        m_handler->releaseBuffer(m_buffer);
    }

private:
    v4l2_buffer m_buffer;
    CameraHandler* m_handler {};
};


#endif // CAMERAHANDLER_H

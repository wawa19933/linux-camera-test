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
    quint64 length {};
};

class VideoFrame;
class FrameBuffer;

enum class PixelFormat {
    RGB = V4L2_PIX_FMT_RGB24,
    BGR = V4L2_PIX_FMT_BGR24,
/* Luminance+Chrominance formats */
    YUYV = V4L2_PIX_FMT_YUYV,
    YYUV = V4L2_PIX_FMT_YYUV,
    YVYU = V4L2_PIX_FMT_YVYU,
    UYVY = V4L2_PIX_FMT_UYVY,
    YUV444 = V4L2_PIX_FMT_YUV444,
    YUV555 = V4L2_PIX_FMT_YUV555,
    YUV565 = V4L2_PIX_FMT_YUV565,
    YUV32 = V4L2_PIX_FMT_YUV32,
/* three planes - Y Cb, Cr */
    YUV410 = V4L2_PIX_FMT_YUV410,
    YUV411P = V4L2_PIX_FMT_YUV411P,
    YUV420 = V4L2_PIX_FMT_YUV420,
    YUV422P = V4L2_PIX_FMT_YUV422P,
    YVU410 = V4L2_PIX_FMT_YVU410,
    YVU420 = V4L2_PIX_FMT_YVU420,
/* three non contiguous planes - Y, Cb, Cr */
    YUV420M = V4L2_PIX_FMT_YUV420M,
    YVU420M = V4L2_PIX_FMT_YVU420M,
    YUV422M = V4L2_PIX_FMT_YUV422M,
    YVU422M = V4L2_PIX_FMT_YVU422M,
/* compressed formats */
    MJPEG = V4L2_PIX_FMT_MJPEG,
    JPEG = V4L2_PIX_FMT_JPEG,
    MPEG = V4L2_PIX_FMT_MPEG,
    MPEG1 = V4L2_PIX_FMT_MPEG1,
    MPEG2 = V4L2_PIX_FMT_MPEG2,
    MPEG4 = V4L2_PIX_FMT_MPEG4,
    H264 = V4L2_PIX_FMT_H264,
    Grey = V4L2_PIX_FMT_GREY
};

enum class Colorspace { Default = V4L2_COLORSPACE_DEFAULT, sRGB = V4L2_COLORSPACE_SRGB,
                       AdobeRGB = V4L2_COLORSPACE_ADOBERGB, JPEG = V4L2_COLORSPACE_JPEG,
                       RAW = V4L2_COLORSPACE_RAW };


class CameraHandler {
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
    QString deviceName () const { return reinterpret_cast<const char*>(m_capability.card); }
    QString driverName () const { return reinterpret_cast<const char*>(m_capability.driver); }
    QString deviceBus () const { return reinterpret_cast<const char*>(m_capability.bus_info); }

    PixelFormat format() const;
    void setFormat(const PixelFormat &format);

    QRect defaultRect () const;
    QSize size() const;
    void setSize(const QSize &size);
    QImage getFrame();
    void close ();
    ~CameraHandler ();

private:
    bool setCropping (const QRect& rect);
    v4l2_format getFmt ();
    v4l2_format setFmt (v4l2_format* fmt);
    void releaseBuffer (const v4l2_buffer& buf);

    bool query (quint32 request, void* arg) const;
    bool openDevice (const char* path);
    bool initialize ();
    bool initBuffers (quint32 count);
    bool getCapabilities ();
    bool enableStreaming ();
    bool disableStreaming ();
    bool releaseBuffer (int index);
    v4l2_buffer takeBuffer ();
    void unmapBuffers ();
    void applySettings ();

private:
    struct v4l2_capability m_capability;
    QVector<Buffer> m_buffers;
//    QRect m_crop { 0, 0, 1280, 720 };
    QSize m_frameSize { 1280, 720 };
    int m_handler {};
    PixelFormat m_pixelFormat { PixelFormat::RGB };
    Colorspace m_colorspace { Colorspace::RAW };

    friend class VideoFrame;
};


//class VideoFrame {
//public:
//    VideoFrame (CameraHandler* handler, v4l2_buffer buf) : m_buffer(buf), m_handler(handler) {}

//    Buffer& buffer () { return m_handler->m_buffers[m_buffer.index]; }

//    const Buffer& buffer () const { return m_handler->m_buffers[m_buffer.index]; }
//    const char* data () const { return (char*) buffer().address; }
//    quint64 size () const { return m_buffer.bytesused; }
//    quint64 length () const { return m_buffer.bytesused; }

//    QImage toImage () const;
////    QPixmap toPixmap () const;

//    ~VideoFrame () {
//        m_handler->releaseBuffer(m_buffer);
//    }

//private:
//    v4l2_buffer m_buffer;
//    CameraHandler* m_handler {};
//};


#endif // CAMERAHANDLER_H

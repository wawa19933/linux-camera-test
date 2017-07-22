#ifndef CAMERAHANDLER_H
#define CAMERAHANDLER_H

#include <QString>
#include <QLoggingCategory>
#include <QSize>
#include <linux/videodev2.h>
#include <fcntl.h>

Q_DECLARE_LOGGING_CATEGORY(camera)

enum class Format { RGB = V4L2_PIX_FMT_RGB24, BGR = V4L2_PIX_FMT_BGR24,
                    Grey = V4L2_PIX_FMT_GREY };

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
        struct v4l2_cropcap crop;
        return (ioctl(m_handler, VIDIOC_S_CROP, &crop) != -1);
    }

    Format format() const;
    void setFormat(const Format &format);

    QSize size() const;
    void setSize(const QSize &size);

private:
    bool initDevice();
    v4l2_format getFormat_ ();
    bool setFormat_ (const v4l2_format* fmt);
private:
    int m_handler {};
    struct v4l2_format m_format;
    struct v4l2_capability m_capability;

//    Format m_format { Format::RGB };
//    QSize m_size { 640, 480 };
};



#endif // CAMERAHANDLER_H

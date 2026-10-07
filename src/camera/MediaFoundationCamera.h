#pragma once
#include "camera/CameraInterface.h"
#include <QImage>
#include <QMutex>

// 前向声明 Windows Media Foundation COM 接口
struct IMFSourceReader;
struct IMFMediaSource;

namespace vi {

// ------------------------------------------------------------------
// 真实摄像头：基于 Windows Media Foundation (IMFSourceReader) 采集
// 无第三方依赖，直接枚举系统摄像头并读取帧，输出 RGB32 QImage。
// 对应 JD「相机SDK使用 / 硬件通信 / 软硬件联调」。
// ------------------------------------------------------------------
class MediaFoundationCamera : public CameraInterface {
public:
    MediaFoundationCamera();
    ~MediaFoundationCamera() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool grabFrame(QImage& frame, int timeoutMs = 1000) override;
    QString name() const override;

private:
    bool startSource();

    IMFSourceReader* reader_ = nullptr;
    IMFMediaSource* source_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    long stride_ = 0;
    bool mfInit_ = false;
    mutable QMutex mutex_;
};

} // namespace vi

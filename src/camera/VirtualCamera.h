#pragma once
#include "camera/CameraInterface.h"
#include <QMutex>

namespace vi {

// ------------------------------------------------------------------
// 虚拟相机：无真实硬件时的模拟采集源
// 每帧实时渲染一张"工件检测场景"：深灰背景 + 亮色圆盘工件，
// 工件上按概率随机出现暗色缺陷点，并叠加轻微噪声模拟真实采集。
// 用于演示完整采集->检测->统计链路，亦可作为单元测试的稳定输入。
// 接入真实相机时新增 CameraInterface 实现即可无缝替换。
// ------------------------------------------------------------------
class VirtualCamera : public CameraInterface {
public:
    explicit VirtualCamera(int width = 640, int height = 480);
    ~VirtualCamera() override = default;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    bool grabFrame(QImage& frame, int timeoutMs = 1000) override;
    QString name() const override;

    int width() const { return width_; }
    int height() const { return height_; }

    // 每帧出现至少一个缺陷的概率 [0,1]
    void setDefectRate(double rate);
    double defectRate() const;

    // 背景噪声幅值 [0,255]
    void setNoise(int noise);
    int noise() const;

    // 渲染一帧（纯函数，供测试直接调用）
    QImage renderFrame(bool withDefects) const;

private:
    int width_;
    int height_;
    bool open_ = false;
    double defectRate_ = 0.5;
    int noise_ = 12;
    mutable QMutex mutex_;
};

} // namespace vi

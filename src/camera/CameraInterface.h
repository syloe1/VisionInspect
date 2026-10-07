#pragma once
#include <QImage>
#include <QString>

namespace vi {

// ------------------------------------------------------------------
// 相机抽象接口（可插拔设计）
// 上层（采集线程/流水线）只依赖此接口，不关心具体相机实现。
// 替换或接入真实工业相机 SDK 时，只需新增一个实现类即可，
// 体现面向对象的多态与依赖倒置，对应 JD「相机 SDK 使用 / 软硬件联调」。
// ------------------------------------------------------------------
class CameraInterface {
public:
    virtual ~CameraInterface() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // 采集一帧图像；超时返回 false，frame 保持不变。
    virtual bool grabFrame(QImage& frame, int timeoutMs = 1000) = 0;

    virtual QString name() const = 0;
};

} // namespace vi

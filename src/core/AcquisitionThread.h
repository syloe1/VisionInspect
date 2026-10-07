#pragma once
#include <QThread>
#include <QAtomicInteger>
#include "core/FrameQueue.h"
#include "camera/CameraInterface.h"

namespace vi {

// ------------------------------------------------------------------
// 采集线程（生产者）：持续从相机采集帧并压入共享队列
// 对应 JD「相机SDK使用 / 多线程采集」
// ------------------------------------------------------------------
class AcquisitionThread : public QThread {
    Q_OBJECT
public:
    AcquisitionThread(CameraInterface* cam, FrameQueue* queue,
                      QObject* parent = nullptr);

    void stop();
    bool isRunning() const { return running_.loadAcquire(); }

signals:
    // 采集到一帧（供 UI 实时显示原始画面）
    void frameCaptured(const QImage& frame);

protected:
    void run() override;

private:
    CameraInterface* cam_;
    FrameQueue* queue_;
    QAtomicInteger<bool> running_{false};
};

} // namespace vi

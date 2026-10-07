#pragma once
#include <QThread>
#include <QAtomicInteger>
#include "core/FrameQueue.h"
#include "vision/Detector.h"
#include "vision/DetectionResult.h"

namespace vi {

// ------------------------------------------------------------------
// 处理线程（消费者）：从队列取帧，执行视觉检测，发出结果
// 与采集线程通过有界队列解耦，实现并行与实时性
// ------------------------------------------------------------------
class ProcessingThread : public QThread {
    Q_OBJECT
public:
    explicit ProcessingThread(FrameQueue* queue, QObject* parent = nullptr);

    void stop();
    bool isRunning() const { return running_.loadAcquire(); }
    const Detector& detector() const { return detector_; }
    Detector& detector() { return detector_; }

signals:
    void resultReady(const DetectionResult& result);

protected:
    void run() override;

private:
    FrameQueue* queue_;
    QAtomicInteger<bool> running_{false};
    Detector detector_;
};

} // namespace vi

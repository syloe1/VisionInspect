#include "core/ProcessingThread.h"

namespace vi {

ProcessingThread::ProcessingThread(FrameQueue* queue, QObject* parent)
    : QThread(parent), queue_(queue) {}

void ProcessingThread::stop() {
    running_.storeRelease(false);
}

void ProcessingThread::run() {
    running_.storeRelease(true);
    while (running_.loadAcquire()) {
        QImage frame;
        // 队列空时最多等待 50ms，随后检查退出标志
        if (queue_->pop(frame, 50)) {
            const DetectionResult r = detector_.detect(frame);
            emit resultReady(r);
        }
    }
}

} // namespace vi

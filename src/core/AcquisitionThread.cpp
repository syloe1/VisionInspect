#include "core/AcquisitionThread.h"

namespace vi {

AcquisitionThread::AcquisitionThread(CameraInterface* cam, FrameQueue* queue,
                                     QObject* parent)
    : QThread(parent), cam_(cam), queue_(queue) {}

void AcquisitionThread::stop() {
    running_.storeRelease(false);
}

void AcquisitionThread::run() {
    running_.storeRelease(true);
    while (running_.loadAcquire()) {
        QImage frame;
        if (cam_->grabFrame(frame)) {
            queue_->push(frame);
            emit frameCaptured(frame);
        }
        // 无显式节流：帧率由相机采集耗时决定
    }
}

} // namespace vi

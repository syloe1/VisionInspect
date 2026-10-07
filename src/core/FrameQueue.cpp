#include "core/FrameQueue.h"

namespace vi {

FrameQueue::FrameQueue(int capacity)
    : capacity_(capacity > 0 ? capacity : 1) {}

void FrameQueue::push(QImage frame) {
    QMutexLocker locker(&mutex_);
    if (buffer_.size() >= capacity_) {
        // 队满：丢弃最旧帧，保持实时性
        buffer_.removeFirst();
    }
    buffer_.append(std::move(frame));
    notEmpty_.wakeOne();
}

bool FrameQueue::pop(QImage& frame, int timeoutMs) {
    QMutexLocker locker(&mutex_);
    while (buffer_.isEmpty()) {
        if (!notEmpty_.wait(&mutex_, timeoutMs)) {
            return false; // 超时仍无数据
        }
    }
    frame = buffer_.takeFirst();
    notFull_.wakeOne();
    return true;
}

int FrameQueue::size() const {
    QMutexLocker locker(&mutex_);
    return buffer_.size();
}

void FrameQueue::clear() {
    QMutexLocker locker(&mutex_);
    buffer_.clear();
    notFull_.wakeAll();
}

} // namespace vi

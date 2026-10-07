#pragma once
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <QList>

namespace vi {

// ------------------------------------------------------------------
// 线程安全的有界帧队列（生产者-消费者）
// 用于解耦「采集线程(生产者)」与「处理线程(消费者)」，
// 是实时视频链路的缓冲核心，对应 JD「编写多线程代码、保证实时性」。
//
// 特性：
//  - 有界容量，避免无限积压造成内存增长与延迟（背压）
//  - 队列满时丢弃最旧帧（实时场景优先新帧，保证实时性）
//  - 消费者等待时支持超时
// ------------------------------------------------------------------
class FrameQueue {
public:
    explicit FrameQueue(int capacity = 4);

    // 生产者：压入一帧。队列满时丢弃最旧帧后压入。
    void push(QImage frame);

    // 消费者：取出一帧。空则等待最多 timeoutMs 毫秒。
    // 返回 true 表示成功取出。
    bool pop(QImage& frame, int timeoutMs = 100);

    int capacity() const { return capacity_; }
    int size() const;
    void clear();

private:
    int capacity_;
    QList<QImage> buffer_;
    mutable QMutex mutex_;
    QWaitCondition notEmpty_;
    QWaitCondition notFull_;
};

} // namespace vi

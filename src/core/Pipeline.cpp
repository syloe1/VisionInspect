#include "core/Pipeline.h"
#include <QElapsedTimer>

namespace vi {

Pipeline::Pipeline(QObject* parent)
    : QObject(parent),
      queue_(std::make_unique<FrameQueue>(4)) {
    // 每秒统计一次帧率
    fpsTimer_.setInterval(1000);
    connect(&fpsTimer_, &QTimer::timeout, this, &Pipeline::onFpsTick);
}

Pipeline::~Pipeline() {
    stop();
}

void Pipeline::setCamera(std::shared_ptr<CameraInterface> cam) {
    if (running_)
        stop();
    camera_ = std::move(cam);
}

Detector& Pipeline::detector() {
    return procThread_ ? procThread_->detector() : stubDetector();
}

const Detector& Pipeline::detector() const {
    return procThread_ ? procThread_->detector() : stubDetector();
}

bool Pipeline::start() {
    if (running_)
        return true;
    if (!camera_)
        return false;
    if (!camera_->open()) {
        emit logMessage(tr("相机打开失败"));
        return false;
    }

    queue_->clear();
    acqThread_ = std::make_unique<AcquisitionThread>(camera_.get(), queue_.get());
    procThread_ = std::make_unique<ProcessingThread>(queue_.get());

    connect(acqThread_.get(), &AcquisitionThread::frameCaptured,
            this, &Pipeline::onFrameCaptured, Qt::QueuedConnection);
    connect(acqThread_.get(), &QThread::finished,
            this, [this] { emit logMessage(tr("采集线程已退出")); });
    connect(procThread_.get(), &ProcessingThread::resultReady,
            this, &Pipeline::resultReady, Qt::QueuedConnection);
    connect(procThread_.get(), &QThread::finished,
            this, [this] { emit logMessage(tr("处理线程已退出")); });

    fpsWindowMs_ = 0;
    fpsFrames_ = 0;
    fps_ = 0.0;
    fpsTimer_.start();

    acqThread_->start();
    procThread_->start();
    running_ = true;
    emit statusChanged(true);
    emit logMessage(tr("流水线启动，相机：%1").arg(camera_->name()));
    return true;
}

void Pipeline::stop() {
    if (!running_)
        return;
    running_ = false;
    fpsTimer_.stop();

    if (acqThread_) {
        acqThread_->stop();
        acqThread_->wait();
    }
    if (procThread_) {
        procThread_->stop();
        procThread_->wait();
    }
    if (camera_)
        camera_->close();

    acqThread_.reset();
    procThread_.reset();
    emit statusChanged(false);
    emit logMessage(tr("流水线已停止"));
}

void Pipeline::onFrameCaptured(const QImage& frame) {
    ++fpsFrames_;
    emit frameReady(frame);
}

void Pipeline::onFpsTick() {
    if (fpsFrames_ > 0) {
        fps_ = double(fpsFrames_);
        fpsFrames_ = 0;
        emit fpsUpdated(fps_);
    }
}

// 供 detector() 在流水线未运行时返回的默认实例
Detector& Pipeline::stubDetector() {
    static Detector s_stub;
    return s_stub;
}

} // namespace vi

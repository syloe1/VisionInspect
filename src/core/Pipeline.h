#pragma once
#include <QObject>
#include <QTimer>
#include <memory>
#include "camera/CameraInterface.h"
#include "core/FrameQueue.h"
#include "core/AcquisitionThread.h"
#include "core/ProcessingThread.h"
#include "vision/DetectionResult.h"

namespace vi {

// ------------------------------------------------------------------
// 采集-处理流水线：组装「相机 + 有界队列 + 采集线程 + 处理线程」
// 对外提供统一的启动/停止/切换相机接口，并转发实时帧与检测结果信号。
// 对应 JD「框架搭建 / 多线程实时性 / 软硬件联调」
// ------------------------------------------------------------------
class Pipeline : public QObject {
    Q_OBJECT
public:
    explicit Pipeline(QObject* parent = nullptr);
    ~Pipeline() override;

    // 设置/切换相机（可插拔，运行时替换）
    void setCamera(std::shared_ptr<CameraInterface> cam);
    std::shared_ptr<CameraInterface> camera() const { return camera_; }

    // 启动/停止流水线
    bool start();
    void stop();
    bool isRunning() const { return running_; }

    // 处理线程的检测器配置入口（供界面调节参数）
    Detector& detector();
    const Detector& detector() const;

    int queuedFrames() const { return queue_ ? queue_->size() : 0; }
    double measuredFps() const { return fps_; }

signals:
    // 原始帧（供界面实时显示）
    void frameReady(const QImage& frame);
    // 检测结果
    void resultReady(const DetectionResult& result);
    // 运行状态变化
    void statusChanged(bool running);
    // 日志
    void logMessage(const QString& msg);
    // 实测帧率（每秒更新一次）
    void fpsUpdated(double fps);

private slots:
    void onFrameCaptured(const QImage& frame);
    void onFpsTick();

private:
    std::shared_ptr<CameraInterface> camera_;
    std::unique_ptr<FrameQueue> queue_;
    std::unique_ptr<AcquisitionThread> acqThread_;
    std::unique_ptr<ProcessingThread> procThread_;
    QTimer fpsTimer_;
    bool running_ = false;

    qint64 fpsWindowMs_ = 0;
    int fpsFrames_ = 0;
    double fps_ = 0.0;

    // 流水线未运行时返回的默认检测器
    static Detector& stubDetector();
};

} // namespace vi

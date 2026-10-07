#pragma once
#include <QMainWindow>
#include "core/Pipeline.h"
#include "comm/TcpReporter.h"
#include "vision/DetectionResult.h"

class QLabel;
class QComboBox;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;

// ------------------------------------------------------------------
// 视觉检测上位机主窗口
// 职责：实时画面显示、检测参数配置、结果统计、TCP 上报、日志
// ------------------------------------------------------------------
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void startPipeline();
    void stopPipeline();
    void applyDetectorParams();
    void connectTcp();
    void disconnectTcp();
    void resetStats();

    void onFrame(const QImage& frame);
    void onResult(const vi::DetectionResult& result);
    void onFps(double fps);
    void onPipelineStatus(bool running);
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpError(const QString& msg);
    void appendLog(const QString& msg);

private:
    void buildUi();
    void updateImage(const QImage& img);
    void setRunningUi(bool running);

    vi::Pipeline pipeline_;
    vi::TcpReporter tcp_;

    // 显示与控制
    QLabel* imageLabel_ = nullptr;
    QComboBox* cameraCombo_ = nullptr;
    QPushButton* startBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
    QLabel* sizeLabel_ = nullptr;

    // 检测参数
    QCheckBox* detectEnabled_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QSpinBox* workLowSpin_ = nullptr;
    QSpinBox* defectHighSpin_ = nullptr;
    QSpinBox* minDefectSpin_ = nullptr;
    QDoubleSpinBox* maxDefectAreaSpin_ = nullptr;
    QSpinBox* maxDefectCountSpin_ = nullptr;

    // TCP 通信
    QLineEdit* hostEdit_ = nullptr;
    QSpinBox* portSpin_ = nullptr;
    QPushButton* tcpConnectBtn_ = nullptr;
    QPushButton* tcpDisconnectBtn_ = nullptr;
    QLabel* tcpStateLabel_ = nullptr;

    // 统计
    QLabel* statOk_ = nullptr;
    QLabel* statNg_ = nullptr;
    QLabel* statTotal_ = nullptr;
    QLabel* statYield_ = nullptr;

    QPlainTextEdit* logView_ = nullptr;

    int okCount_ = 0;
    int ngCount_ = 0;
};

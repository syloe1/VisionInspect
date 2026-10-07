#include "ui/MainWindow.h"
#include "camera/VirtualCamera.h"
#include "camera/MediaFoundationCamera.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDockWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QPixmap>
#include <QScrollArea>
#include <QDateTime>

using namespace vi;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("VisionInspect - 工业视觉检测上位机"));
    resize(1180, 720);
    buildUi();

    // 连接流水线信号
    connect(&pipeline_, &Pipeline::frameReady, this, &MainWindow::onFrame);
    connect(&pipeline_, &Pipeline::resultReady, this, &MainWindow::onResult);
    connect(&pipeline_, &Pipeline::fpsUpdated, this, &MainWindow::onFps);
    connect(&pipeline_, &Pipeline::statusChanged, this, &MainWindow::onPipelineStatus);
    connect(&pipeline_, &Pipeline::logMessage, this, &MainWindow::appendLog);

    // 连接 TCP 信号
    connect(&tcp_, &TcpReporter::connected, this, &MainWindow::onTcpConnected);
    connect(&tcp_, &TcpReporter::disconnected, this, &MainWindow::onTcpDisconnected);
    connect(&tcp_, &TcpReporter::errorOccurred, this, &MainWindow::onTcpError);
    connect(&tcp_, &TcpReporter::logMessage, this, &MainWindow::appendLog);

    // 默认相机
    pipeline_.setCamera(std::make_shared<VirtualCamera>(640, 480));
    appendLog(tr("就绪。点击「开始采集」启动虚拟相机检测流水线。"));
}

void MainWindow::buildUi() {
    // ---------- 顶部工具栏 ----------
    QToolBar* tb = addToolBar(QStringLiteral("主控制"));
    tb->setMovable(false);
    startBtn_ = new QPushButton(QStringLiteral("开始采集"), this);
    stopBtn_ = new QPushButton(QStringLiteral("停止"), this);
    stopBtn_->setEnabled(false);
    cameraCombo_ = new QComboBox(this);
    cameraCombo_->addItem(QStringLiteral("虚拟相机 (VirtualCamera)"));
    cameraCombo_->addItem(QStringLiteral("真实摄像头 (MediaFoundation)"));
    tb->addWidget(new QLabel(QStringLiteral(" 相机: "), this));
    tb->addWidget(cameraCombo_);
    tb->addWidget(startBtn_);
    tb->addWidget(stopBtn_);

    connect(startBtn_, &QPushButton::clicked, this, &MainWindow::startPipeline);
    connect(stopBtn_, &QPushButton::clicked, this, &MainWindow::stopPipeline);

    // ---------- 中央：实时画面 ----------
    imageLabel_ = new QLabel(this);
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setMinimumSize(480, 360);
    imageLabel_->setStyleSheet(QStringLiteral("background:#202020; border:1px solid #444;"));
    imageLabel_->setText(QStringLiteral("等待图像..."));

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidget(imageLabel_);
    scroll->setWidgetResizable(true);
    scroll->setAlignment(Qt::AlignCenter);
    setCentralWidget(scroll);

    fpsLabel_ = new QLabel(QStringLiteral("FPS: -"), this);
    sizeLabel_ = new QLabel(QStringLiteral("图像: -"), this);
    statusBar()->addPermanentWidget(fpsLabel_);
    statusBar()->addPermanentWidget(sizeLabel_);

    // ---------- 右侧：控制面板 ----------
    QWidget* right = new QWidget(this);
    QVBoxLayout* rl = new QVBoxLayout(right);

    // 检测参数
    QGroupBox* detBox = new QGroupBox(QStringLiteral("检测参数"), right);
    QFormLayout* detForm = new QFormLayout(detBox);
    detectEnabled_ = new QCheckBox(QStringLiteral("启用检测并计入统计"), detBox);
    detectEnabled_->setChecked(true);
    modeCombo_ = new QComboBox(detBox);
    modeCombo_->addItem(QStringLiteral("工件检测 (WorkInspection)"));
    modeCombo_->addItem(QStringLiteral("人脸检测 (FaceDetection)"));
    workLowSpin_ = new QSpinBox(detBox);
    workLowSpin_->setRange(0, 255);
    workLowSpin_->setValue(pipeline_.detector().config().workLow);
    defectHighSpin_ = new QSpinBox(detBox);
    defectHighSpin_->setRange(0, 255);
    defectHighSpin_->setValue(pipeline_.detector().config().defectHigh);
    minDefectSpin_ = new QSpinBox(detBox);
    minDefectSpin_->setRange(0, 500);
    minDefectSpin_->setValue(pipeline_.detector().config().minDefectArea);
    maxDefectAreaSpin_ = new QDoubleSpinBox(detBox);
    maxDefectAreaSpin_->setRange(0, 100000);
    maxDefectAreaSpin_->setDecimals(1);
    maxDefectAreaSpin_->setValue(pipeline_.detector().config().maxDefectArea);
    maxDefectCountSpin_ = new QSpinBox(detBox);
    maxDefectCountSpin_->setRange(0, 100);
    maxDefectCountSpin_->setValue(pipeline_.detector().config().maxDefectCount);
    detForm->addRow(QStringLiteral("启用检测"), detectEnabled_);
    detForm->addRow(QStringLiteral("检测模式"), modeCombo_);
    detForm->addRow(QStringLiteral("工件阈值下限"), workLowSpin_);
    detForm->addRow(QStringLiteral("缺陷阈值上限"), defectHighSpin_);
    detForm->addRow(QStringLiteral("缺陷最小面积"), minDefectSpin_);
    detForm->addRow(QStringLiteral("NG缺陷面积上限"), maxDefectAreaSpin_);
    detForm->addRow(QStringLiteral("NG缺陷数量上限"), maxDefectCountSpin_);
    QPushButton* applyBtn = new QPushButton(QStringLiteral("应用参数"), detBox);
    detForm->addRow(applyBtn);
    connect(applyBtn, &QPushButton::clicked, this, &MainWindow::applyDetectorParams);

    // TCP 通信
    QGroupBox* tcpBox = new QGroupBox(QStringLiteral("结果上报 (TCP)"), right);
    QFormLayout* tcpForm = new QFormLayout(tcpBox);
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"), tcpBox);
    portSpin_ = new QSpinBox(tcpBox);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(8000);
    tcpConnectBtn_ = new QPushButton(QStringLiteral("连接"), tcpBox);
    tcpDisconnectBtn_ = new QPushButton(QStringLiteral("断开"), tcpBox);
    tcpDisconnectBtn_->setEnabled(false);
    tcpStateLabel_ = new QLabel(QStringLiteral("未连接"), tcpBox);
    QHBoxLayout* tcpBtns = new QHBoxLayout;
    tcpBtns->addWidget(tcpConnectBtn_);
    tcpBtns->addWidget(tcpDisconnectBtn_);
    tcpForm->addRow(QStringLiteral("服务器地址"), hostEdit_);
    tcpForm->addRow(QStringLiteral("端口"), portSpin_);
    tcpForm->addRow(tcpBtns);
    tcpForm->addRow(QStringLiteral("状态"), tcpStateLabel_);
    connect(tcpConnectBtn_, &QPushButton::clicked, this, &MainWindow::connectTcp);
    connect(tcpDisconnectBtn_, &QPushButton::clicked, this, &MainWindow::disconnectTcp);

    // 统计
    QGroupBox* statBox = new QGroupBox(QStringLiteral("检测统计"), right);
    QFormLayout* statForm = new QFormLayout(statBox);
    statOk_ = new QLabel(QStringLiteral("0"), statBox);
    statNg_ = new QLabel(QStringLiteral("0"), statBox);
    statTotal_ = new QLabel(QStringLiteral("0"), statBox);
    statYield_ = new QLabel(QStringLiteral("0%"), statBox);
    QPushButton* resetBtn = new QPushButton(QStringLiteral("清零"), statBox);
    statForm->addRow(QStringLiteral("OK"), statOk_);
    statForm->addRow(QStringLiteral("NG"), statNg_);
    statForm->addRow(QStringLiteral("总数"), statTotal_);
    statForm->addRow(QStringLiteral("良率"), statYield_);
    statForm->addRow(resetBtn);
    connect(resetBtn, &QPushButton::clicked, this, &MainWindow::resetStats);

    rl->addWidget(detBox);
    rl->addWidget(tcpBox);
    rl->addWidget(statBox);
    rl->addStretch();

    QDockWidget* dock = new QDockWidget(QStringLiteral("控制面板"), this);
    dock->setWidget(right);
    dock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // ---------- 底部：日志 ----------
    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(1000);
    QDockWidget* logDock = new QDockWidget(QStringLiteral("日志"), this);
    logDock->setWidget(logView_);
    logDock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
}

void MainWindow::startPipeline() {
    if (pipeline_.isRunning())
        return;
    // 按界面选择创建相机：0=虚拟相机，1=真实摄像头
    if (cameraCombo_->currentIndex() == 1) {
        pipeline_.setCamera(std::make_shared<MediaFoundationCamera>());
    } else {
        pipeline_.setCamera(std::make_shared<VirtualCamera>(640, 480));
    }
    if (pipeline_.start())
        setRunningUi(true);
}

void MainWindow::stopPipeline() {
    pipeline_.stop();
    setRunningUi(false);
}

void MainWindow::applyDetectorParams() {
    // 检测模式：0=工件检测，1=人脸检测
    pipeline_.detector().setMode(modeCombo_->currentIndex() == 1
                                     ? DetectionMode::FaceDetection
                                     : DetectionMode::WorkInspection);
    DetectorConfig cfg = pipeline_.detector().config();
    cfg.workLow = workLowSpin_->value();
    cfg.defectHigh = defectHighSpin_->value();
    cfg.minDefectArea = minDefectSpin_->value();
    cfg.maxDefectArea = maxDefectAreaSpin_->value();
    cfg.maxDefectCount = maxDefectCountSpin_->value();
    pipeline_.detector().setConfig(cfg);
    appendLog(tr("检测参数已更新：mode=%1, workLow=%2, defectHigh=%3, minDefect=%4, "
                 "maxDefectArea=%5, maxDefectCount=%6")
                  .arg(modeCombo_->currentIndex() == 1
                           ? QStringLiteral("人脸检测")
                           : QStringLiteral("工件检测"))
                  .arg(cfg.workLow).arg(cfg.defectHigh).arg(cfg.minDefectArea)
                  .arg(cfg.maxDefectArea).arg(cfg.maxDefectCount));
}

void MainWindow::connectTcp() {
    tcp_.setAutoReconnect(true);
    tcp_.connectTo(hostEdit_->text().trimmed(), quint16(portSpin_->value()));
}

void MainWindow::disconnectTcp() {
    tcp_.setAutoReconnect(false);
    tcp_.disconnectFromHost();
    tcpStateLabel_->setText(QStringLiteral("未连接"));
}

void MainWindow::resetStats() {
    okCount_ = 0;
    ngCount_ = 0;
    statOk_->setText(QStringLiteral("0"));
    statNg_->setText(QStringLiteral("0"));
    statTotal_->setText(QStringLiteral("0"));
    statYield_->setText(QStringLiteral("0%"));
}

void MainWindow::onFrame(const QImage& frame) {
    sizeLabel_->setText(tr("图像: %1x%2").arg(frame.width()).arg(frame.height()));
    // 仅当未启用检测结果覆盖时显示原图
    if (!detectEnabled_->isChecked())
        updateImage(frame);
}

void MainWindow::onResult(const DetectionResult& result) {
    if (!detectEnabled_->isChecked())
        return;

    // 人脸模式：不适用 OK/NG 统计，改为显示检出人脸数
    if (result.mode == DetectionMode::FaceDetection) {
        statOk_->setText(QStringLiteral("脸"));
        statNg_->setText(QStringLiteral("帧"));
        statTotal_->setText(QString::number(result.faces.size()));
        statYield_->setText(tr("%1 张").arg(result.faces.size()));
        if (!result.annotated.isNull())
            updateImage(result.annotated);
        appendLog(tr("人脸检测: 检出 %1 张").arg(result.faces.size()));
        return;
    }

    // 更新统计
    ++(result.ok ? okCount_ : ngCount_);
    const int total = okCount_ + ngCount_;
    statOk_->setText(QString::number(okCount_));
    statNg_->setText(QString::number(ngCount_));
    statTotal_->setText(QString::number(total));
    if (total > 0) {
        statYield_->setText(tr("%1%").arg(double(okCount_) * 100.0 / total, 0, 'f', 1));
    }

    // 显示叠加标记的结果图
    if (!result.annotated.isNull())
        updateImage(result.annotated);

    // 结果上报 TCP
    tcp_.sendResult(result);

    appendLog(tr("检测: %1 | 缺陷数=%2 | 缺陷面积=%3 | 工件(%4,%5 %6x%7)")
                  .arg(result.ok ? QStringLiteral("OK") : QStringLiteral("NG"))
                  .arg(result.defectCount)
                  .arg(result.defectArea, 0, 'f', 1)
                  .arg(result.workArea.x()).arg(result.workArea.y())
                  .arg(result.workArea.width()).arg(result.workArea.height()));
}

void MainWindow::onFps(double fps) {
    fpsLabel_->setText(tr("FPS: %1").arg(fps, 0, 'f', 1));
}

void MainWindow::onPipelineStatus(bool running) {
    setRunningUi(running);
}

void MainWindow::onTcpConnected() {
    tcpStateLabel_->setText(QStringLiteral("已连接"));
    tcpConnectBtn_->setEnabled(false);
    tcpDisconnectBtn_->setEnabled(true);
}

void MainWindow::onTcpDisconnected() {
    tcpStateLabel_->setText(QStringLiteral("未连接"));
    tcpConnectBtn_->setEnabled(true);
    tcpDisconnectBtn_->setEnabled(false);
}

void MainWindow::onTcpError(const QString& msg) {
    tcpStateLabel_->setText(QStringLiteral("连接失败"));
    tcpConnectBtn_->setEnabled(true);
    tcpDisconnectBtn_->setEnabled(false);
    Q_UNUSED(msg)
}

void MainWindow::appendLog(const QString& msg) {
    logView_->appendPlainText(
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz ")) + msg);
}

void MainWindow::updateImage(const QImage& img) {
    if (img.isNull())
        return;
    const QPixmap pm = QPixmap::fromImage(img);
    const QSize labelSize = imageLabel_->size();
    if (labelSize.width() > 0 && labelSize.height() > 0) {
        imageLabel_->setPixmap(pm.scaled(labelSize, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
    } else {
        imageLabel_->setPixmap(pm);
    }
}

void MainWindow::setRunningUi(bool running) {
    startBtn_->setEnabled(!running);
    stopBtn_->setEnabled(running);
    cameraCombo_->setEnabled(!running);
}

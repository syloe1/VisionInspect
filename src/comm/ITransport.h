#pragma once
#include <QObject>
#include <QJsonObject>
#include "vision/DetectionResult.h"

namespace vi {

// ------------------------------------------------------------------
// 传输层抽象接口：负责把检测结果可靠地发送给远端（PLC / 服务器）。
//
// 设计目的（与相机层 CameraInterface 一脉相承，遵循依赖倒置）：
//   - 上层（MainWindow / 业务逻辑）只依赖本接口，不依赖具体协议实现；
//   - 目前默认实现为 TcpReporter（TCP，适用于局域网 / 结果上报）；
//   - 未来可扩展 KcpReporter 等低延迟可靠传输（如实时视频帧传输、
//     广域网低延迟控制场景），上层零改动。
//
// 本接口为"纯重构"产物：仅抽出公共传输语义，不改变任何现有 TCP 行为。
// ------------------------------------------------------------------
class ITransport : public QObject {
    Q_OBJECT
public:
    explicit ITransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ITransport() override = default;

    // 建立连接到远端 host:port
    virtual void connectTo(const QString& host, quint16 port) = 0;
    // 断开连接
    virtual void disconnectFromHost() = 0;
    // 是否已连接
    virtual bool isConnected() const = 0;
    // 是否启用断线自动重连（具体实现按自身能力生效）
    virtual void setAutoReconnect(bool on) = 0;

    // 发送一次检测结果（内部转换为 JSON 并组帧发送）
    virtual bool sendResult(const DetectionResult& result) = 0;
    // 发送任意 JSON 对象
    virtual bool sendJson(const QJsonObject& obj) = 0;

signals:
    // 连接建立成功
    void connected();
    // 连接断开
    void disconnected();
    // 传输层错误
    void errorOccurred(const QString& msg);
    // 日志消息（供 UI 展示）
    void logMessage(const QString& msg);
};

} // namespace vi

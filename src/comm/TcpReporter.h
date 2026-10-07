#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QTimer>
#include "vision/DetectionResult.h"

namespace vi {

// ------------------------------------------------------------------
// TCP 结果上报模块：将检测结果按自定义协议发送给 PLC / 上位服务
//
// 协议帧格式（对应 JD「网络编程 / 硬件通信调试」）：
//   [0xAA 0x55] [payload_len: uint32 LE] [payload: JSON] [crc16: uint16]
//   crc16 覆盖 payload_len 与 payload。
// 支持断线自动重连。
// ------------------------------------------------------------------
class TcpReporter : public QObject {
    Q_OBJECT
public:
    explicit TcpReporter(QObject* parent = nullptr);

    void connectTo(const QString& host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    void setAutoReconnect(bool on);
    bool autoReconnect() const { return autoReconnect_; }

    // 发送一次检测结果（内部转换为 JSON 并组帧发送）
    bool sendResult(const DetectionResult& result);
    // 发送任意 JSON 对象
    bool sendJson(const QJsonObject& obj);

    // 将检测结果序列化为协议 payload（供单测校验协议正确性）
    static QByteArray resultToPayload(const DetectionResult& result);
    // 组帧（供单测校验）
    static QByteArray buildPacket(const QByteArray& payload);
    // CRC-16/CCITT (0x1021)
    static quint16 crc16(const QByteArray& data);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& msg);
    void logMessage(const QString& msg);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError err);
    void onReconnectTick();

private:
    void scheduleReconnect();

    QTcpSocket* socket_;
    QTimer reconnectTimer_;
    QString host_;
    quint16 port_ = 0;
    bool autoReconnect_ = true;
};

} // namespace vi

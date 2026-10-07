#pragma once
#include <QTcpSocket>
#include <QTimer>
#include "comm/ITransport.h"

namespace vi {

// ------------------------------------------------------------------
// TCP 结果上报模块：将检测结果按自定义协议发送给 PLC / 上位服务
//
// 协议帧格式（对应 JD「网络编程 / 硬件通信调试」）：
//   [0xAA 0x55] [payload_len: uint32 LE] [payload: JSON] [crc16: uint16]
//   crc16 覆盖 payload_len 与 payload。
// 支持断线自动重连。
//
// 实现 ITransport 传输接口：上层只依赖接口，本类为默认 TCP 实现。
// 静态协议方法（resultToPayload / buildPacket / crc16）保留于此，
// 属 TCP 协议层细节，不进入通用传输接口。
// ------------------------------------------------------------------
class TcpReporter : public ITransport {
    Q_OBJECT
public:
    explicit TcpReporter(QObject* parent = nullptr);

    // ---- ITransport 接口实现 ----
    void connectTo(const QString& host, quint16 port) override;
    void disconnectFromHost() override;
    bool isConnected() const override;
    void setAutoReconnect(bool on) override;

    // 发送一次检测结果（内部转换为 JSON 并组帧发送）
    bool sendResult(const DetectionResult& result) override;
    // 发送任意 JSON 对象
    bool sendJson(const QJsonObject& obj) override;

    bool autoReconnect() const { return autoReconnect_; }

    // 将检测结果序列化为协议 payload（供单测校验协议正确性）
    static QByteArray resultToPayload(const DetectionResult& result);
    // 组帧（供单测校验）
    static QByteArray buildPacket(const QByteArray& payload);
    // CRC-16/CCITT (0x1021)
    static quint16 crc16(const QByteArray& data);

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

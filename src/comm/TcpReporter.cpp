#include "comm/TcpReporter.h"
#include <QJsonDocument>
#include <QDataStream>

namespace vi {

namespace {
// 协议帧头
constexpr quint8 kHead0 = 0xAA;
constexpr quint8 kHead1 = 0x55;
} // namespace

TcpReporter::TcpReporter(QObject* parent)
    : QObject(parent), socket_(new QTcpSocket(this)) {
    connect(socket_, &QTcpSocket::connected, this, &TcpReporter::onSocketConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &TcpReporter::onSocketDisconnected);
    connect(socket_, &QAbstractSocket::errorOccurred, this, &TcpReporter::onSocketError);

    reconnectTimer_.setSingleShot(true);
    connect(&reconnectTimer_, &QTimer::timeout, this, &TcpReporter::onReconnectTick);
}

void TcpReporter::connectTo(const QString& host, quint16 port) {
    host_ = host;
    port_ = port;
    if (socket_->state() == QAbstractSocket::ConnectedState ||
        socket_->state() == QAbstractSocket::ConnectingState) {
        socket_->abort();
    }
    emit logMessage(tr("正在连接 %1:%2 ...").arg(host).arg(port));
    socket_->connectToHost(host, port);
}

void TcpReporter::disconnectFromHost() {
    reconnectTimer_.stop();
    if (socket_->state() != QAbstractSocket::UnconnectedState)
        socket_->disconnectFromHost();
}

bool TcpReporter::isConnected() const {
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void TcpReporter::setAutoReconnect(bool on) {
    autoReconnect_ = on;
    if (!on)
        reconnectTimer_.stop();
}

bool TcpReporter::sendJson(const QJsonObject& obj) {
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        emit logMessage(tr("TCP 未连接，丢弃数据"));
        return false;
    }
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    const QByteArray packet = buildPacket(payload);
    const qint64 n = socket_->write(packet);
    return n == packet.size();
}

bool TcpReporter::sendResult(const DetectionResult& result) {
    return sendJson(QJsonDocument::fromJson(resultToPayload(result)).object());
}

QByteArray TcpReporter::resultToPayload(const DetectionResult& r) {
    QJsonObject obj;
    obj[QStringLiteral("type")] = QStringLiteral("inspection_result");
    obj[QStringLiteral("ok")] = r.ok;
    obj[QStringLiteral("work_found")] = r.workFound;
    obj[QStringLiteral("defect_count")] = r.defectCount;
    obj[QStringLiteral("defect_area")] = r.defectArea;
    obj[QStringLiteral("timestamp_ms")] = r.timestampMs;
    if (r.workFound) {
        obj[QStringLiteral("work_x")] = r.workArea.x();
        obj[QStringLiteral("work_y")] = r.workArea.y();
        obj[QStringLiteral("work_w")] = r.workArea.width();
        obj[QStringLiteral("work_h")] = r.workArea.height();
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

quint16 TcpReporter::crc16(const QByteArray& data) {
    quint16 crc = 0xFFFF;
    for (char c : data) {
        crc ^= (quint8(c) << 8);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000)
                crc = quint16((crc << 1) ^ 0x1021);
            else
                crc = quint16(crc << 1);
        }
    }
    return crc;
}

QByteArray TcpReporter::buildPacket(const QByteArray& payload) {
    QByteArray packet;
    packet.append(char(kHead0));
    packet.append(char(kHead1));
    // payload_len: uint32 LE
    QByteArray len(4, 0);
    len[0] = char(payload.size() & 0xFF);
    len[1] = char((payload.size() >> 8) & 0xFF);
    len[2] = char((payload.size() >> 16) & 0xFF);
    len[3] = char((payload.size() >> 24) & 0xFF);
    packet.append(len);
    packet.append(payload);
    // crc16 over (len + payload)
    const QByteArray crcInput = len + payload;
    const quint16 crc = crc16(crcInput);
    packet.append(char(crc & 0xFF));
    packet.append(char((crc >> 8) & 0xFF));
    return packet;
}

void TcpReporter::onSocketConnected() {
    reconnectTimer_.stop();
    emit connected();
    emit logMessage(tr("TCP 已连接 %1:%2").arg(host_).arg(port_));
}

void TcpReporter::onSocketDisconnected() {
    emit disconnected();
    emit logMessage(tr("TCP 连接断开"));
    if (autoReconnect_ && socket_->state() == QAbstractSocket::UnconnectedState)
        scheduleReconnect();
}

void TcpReporter::onSocketError(QAbstractSocket::SocketError err) {
    QString desc = socket_->errorString();
    emit errorOccurred(desc);
    emit logMessage(tr("TCP 错误：%1").arg(desc));
    // 非主动断开时安排重连
    if (err != QAbstractSocket::RemoteHostClosedError &&
        err != QAbstractSocket::ConnectionRefusedError) {
        if (autoReconnect_)
            scheduleReconnect();
    }
}

void TcpReporter::onReconnectTick() {
    if (host_.isEmpty() || port_ == 0)
        return;
    if (socket_->state() != QAbstractSocket::UnconnectedState)
        return;
    emit logMessage(tr("尝试重连 %1:%2 ...").arg(host_).arg(port_));
    socket_->connectToHost(host_, port_);
}

void TcpReporter::scheduleReconnect() {
    if (autoReconnect_ && !reconnectTimer_.isActive()) {
        reconnectTimer_.start(3000); // 3 秒后重连
    }
}

} // namespace vi

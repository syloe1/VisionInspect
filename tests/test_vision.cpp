#include <QtTest>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "vision/ImageAlgorithms.h"
#include "vision/Detector.h"
#include "core/FrameQueue.h"
#include "comm/TcpReporter.h"

using namespace vi;

// ------------------------------------------------------------------
// 单元/集成测试：视觉算法、检测器、帧队列、TCP 协议
// 对应 JD「编写单元测试和集成测试用例」
// ------------------------------------------------------------------
class TestVision : public QObject {
    Q_OBJECT
private slots:
    void grayscaleTest();
    void thresholdTest();
    void connectedComponentsTest();
    void detectorOkTest();
    void detectorNgTest();
    void detectorMissingWorkTest();
    void faceDetectionTest();
    void frameQueueTest();
    void tcpProtocolTest();
};

// 构造一张灰度图：背景灰 60，中心亮圆盘(200) 半径 r，圆内可选暗缺陷
static QImage makeWorkImage(int w, int h, int cx, int cy, int r,
                            bool withDefect = false) {
    QImage img(w, h, QImage::Format_Grayscale8);
    img.fill(60); // 背景
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r * r)
                img.setPixelColor(x, y, QColor(200, 200, 200)); // 工件亮区
        }
    }
    if (withDefect) {
        // 在工件内部放一个暗缺陷
        for (int y = cy - 5; y <= cy + 5; ++y)
            for (int x = cx - 5; x <= cx + 5; ++x)
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= 25)
                    img.setPixelColor(x, y, QColor(20, 20, 20));
    }
    return img;
}

void TestVision::grayscaleTest() {
    QImage rgb(4, 4, QImage::Format_RGB32);
    rgb.fill(QColor(128, 128, 128)); // 纯灰，灰度必为 128
    QImage g = ImageAlgorithms::grayscale(rgb);
    QVERIFY(!g.isNull());
    QCOMPARE(g.format(), QImage::Format_Grayscale8);
    QCOMPARE(g.pixelColor(0, 0).red(), 128);
}

void TestVision::thresholdTest() {
    QImage gray(3, 3, QImage::Format_Grayscale8);
    gray.fill(0);
    gray.setPixelColor(0, 0, QColor(100, 100, 100));
    gray.setPixelColor(1, 1, QColor(200, 200, 200));
    QImage out = ImageAlgorithms::threshold(gray, 50, 150);
    QCOMPARE(out.pixelColor(0, 0).red(), 255); // 100∈[50,150] -> on
    QCOMPARE(out.pixelColor(1, 1).red(), 0);   // 200 超出 -> off
    QCOMPARE(out.pixelColor(2, 2).red(), 0);   // 0 低于 -> off
}

void TestVision::connectedComponentsTest() {
    // 5x5：中心 3x3 方块(面积9) + 左上孤立点(面积1)
    QImage bin(5, 5, QImage::Format_Grayscale8);
    bin.fill(0);
    for (int y = 2; y <= 4; ++y)
        for (int x = 2; x <= 4; ++x)
            bin.setPixelColor(x, y, QColor(255, 255, 255));
    bin.setPixelColor(0, 0, QColor(255, 255, 255));

    auto comps = ImageAlgorithms::connectedComponents(bin, 0);
    QCOMPARE(comps.size(), 2); // 8邻域下两个分量互不连通
    int maxArea = 0;
    for (const auto& c : comps)
        maxArea = qMax(maxArea, c.area);
    QCOMPARE(maxArea, 9);
}

void TestVision::detectorOkTest() {
    Detector det;
    const QImage img = makeWorkImage(200, 200, 100, 100, 60, /*withDefect=*/false);
    const DetectionResult r = det.detect(img);
    QVERIFY(r.workFound);
    QVERIFY(r.ok);            // 无缺陷 -> OK
    QCOMPARE(r.defectCount, 0);
    QVERIFY(!r.annotated.isNull());
}

void TestVision::detectorNgTest() {
    Detector det;
    const QImage img = makeWorkImage(200, 200, 100, 100, 60, /*withDefect=*/true);
    const DetectionResult r = det.detect(img);
    QVERIFY(r.workFound);
    QVERIFY(!r.ok);           // 存在缺陷 -> NG
    QVERIFY(r.defectCount >= 1);
    QVERIFY(r.defectArea > 0);
}

void TestVision::detectorMissingWorkTest() {
    Detector det;
    QImage blank(200, 200, QImage::Format_Grayscale8);
    blank.fill(60); // 纯背景，无工件
    const DetectionResult r = det.detect(blank);
    QVERIFY(!r.workFound);
    QVERIFY(!r.ok);           // 未找到工件 -> NG
}

void TestVision::faceDetectionTest() {
    Detector det;
    det.setMode(DetectionMode::FaceDetection);
    // 灰色背景 + 中部一块肤色矩形
    QImage img(200, 200, QImage::Format_RGB32);
    img.fill(QColor(60, 60, 60));
    for (int y = 80; y <= 120; ++y)
        for (int x = 80; x <= 110; ++x)
            img.setPixelColor(x, y, QColor(200, 130, 100)); // 肤色
    const DetectionResult r = det.detect(img);
    QCOMPARE(r.mode, DetectionMode::FaceDetection);
    QVERIFY(!r.faces.isEmpty());       // 检出人脸框
    QVERIFY(r.faces.first().width() > 0);
    QVERIFY(!r.annotated.isNull());
}

void TestVision::frameQueueTest() {
    FrameQueue q(2);
    QVERIFY(q.size() == 0);

    QImage a(8, 8, QImage::Format_Grayscale8); a.fill(10);
    QImage b(8, 8, QImage::Format_Grayscale8); b.fill(20);
    QImage c(8, 8, QImage::Format_Grayscale8); c.fill(30);

    q.push(a);
    q.push(b);
    QCOMPARE(q.size(), 2);

    q.push(c); // 满 -> 丢弃最旧 a
    QCOMPARE(q.size(), 2);

    QImage out;
    QVERIFY(q.pop(out));
    QCOMPARE(out.pixelColor(0, 0).red(), 20); // 先出 b
    QVERIFY(q.pop(out));
    QCOMPARE(out.pixelColor(0, 0).red(), 30); // 再出 c
    QVERIFY(!q.pop(out, 5));                  // 空且超时 -> false
}

void TestVision::tcpProtocolTest() {
    DetectionResult r;
    r.ok = false;
    r.workFound = true;
    r.defectCount = 2;
    r.defectArea = 50.5;
    r.workArea = QRect(10, 20, 30, 40);
    r.timestampMs = 1234567890;

    // payload 是合法 JSON 且字段正确
    const QByteArray payload = TcpReporter::resultToPayload(r);
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    QCOMPARE(err.error, QJsonParseError::NoError);
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object()[QStringLiteral("ok")].toBool(), false);
    QCOMPARE(doc.object()[QStringLiteral("defect_count")].toInt(), 2);
    QVERIFY(qAbs(doc.object()[QStringLiteral("defect_area")].toDouble() - 50.5) < 1e-6);

    // 帧结构：2 字节头 + 4 字节长度 + payload + 2 字节 CRC
    const QByteArray packet = TcpReporter::buildPacket(payload);
    QCOMPARE(packet.size(), 2 + 4 + payload.size() + 2);
    QCOMPARE(int(uchar(packet[0])), 0xAA);
    QCOMPARE(int(uchar(packet[1])), 0x55);

    const int len = uchar(packet[2]) | (uchar(packet[3]) << 8) |
                    (uchar(packet[4]) << 16) | (uchar(packet[5]) << 24);
    QCOMPARE(len, payload.size());

    // CRC 一致
    const quint16 computed =
        TcpReporter::crc16(packet.mid(2, 4 + payload.size()));
    const quint16 stored = uchar(packet[packet.size() - 2]) |
                           (uchar(packet[packet.size() - 1]) << 8);
    QCOMPARE(computed, stored);
}

QTEST_MAIN(TestVision)
#include "test_vision.moc"

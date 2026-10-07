#include "vision/Detector.h"
#include "vision/ImageAlgorithms.h"
#include <QDateTime>
#include <QPainter>
#include <QPen>

namespace vi {

Detector::Detector() = default;

void Detector::setConfig(const DetectorConfig& cfg) {
    cfg_ = cfg;
}

const DetectorConfig& Detector::config() const {
    return cfg_;
}

DetectionResult Detector::detect(const QImage& frame) {
    // 人脸模式走独立检测流程
    if (mode_ == DetectionMode::FaceDetection)
        return runFaceDetection(frame);

    DetectionResult res;
    res.mode = DetectionMode::WorkInspection;
    res.timestampMs = QDateTime::currentMSecsSinceEpoch();

    if (frame.isNull()) {
        res.ok = false;
        return res;
    }

    // 1. 灰度化
    const QImage gray = ImageAlgorithms::grayscale(frame);

    // 2. 阈值分割提取亮区 -> 连通域，取最大分量作为工件
    const QImage bright = ImageAlgorithms::threshold(gray, cfg_.workLow, 255);
    const auto workComps =
        ImageAlgorithms::connectedComponents(bright, cfg_.minWorkArea);

    if (workComps.isEmpty()) {
        // 未定位到工件
        res.ok = false;
        res.workFound = false;
        res.annotated = frame.convertToFormat(QImage::Format_RGB32);
        return res;
    }

    // 最大连通域 = 工件
    const auto* workComp = &workComps.first();
    for (const auto& c : workComps) {
        if (c.area > workComp->area)
            workComp = &c;
    }
    res.workFound = true;
    res.workArea = workComp->bounds;

    // 3. 在工件包围盒内检测暗缺陷
    const QRect wa = res.workArea;
    if (wa.isValid() && !wa.isEmpty()) {
        QImage roi = gray.copy(wa);
        const QImage dark =
            ImageAlgorithms::threshold(roi, 0, cfg_.defectHigh);
        const auto defComps =
            ImageAlgorithms::connectedComponents(dark, cfg_.minDefectArea);

        res.defectCount = defComps.size();
        double totalArea = 0.0;
        for (const auto& d : defComps) {
            DefectInfo info;
            info.pos = QPoint(int(d.cx), int(d.cy));
            info.area = d.area;
            info.bounds = d.bounds.translated(wa.topLeft());
            totalArea += d.area;
            res.defects.append(info);
        }
        res.defectArea = totalArea;
    }

    // 4. OK/NG 判定
    res.ok = res.workFound &&
             res.defectCount <= cfg_.maxDefectCount &&
             res.defectArea <= cfg_.maxDefectArea;

    // 5. 生成叠加标记的结果图（绿框=OK，红框=NG；缺陷红点）
    QImage annotated = frame.convertToFormat(QImage::Format_RGB32);
    QPainter p(&annotated);
    p.setPen(QPen(res.ok ? QColor(0, 200, 0) : QColor(220, 0, 0), 3));
    p.drawRect(wa);
    p.setPen(QPen(QColor(255, 0, 0), 2));
    p.setBrush(Qt::NoBrush);
    for (const auto& d : res.defects) {
        const QRect b = d.bounds;
        p.drawEllipse(b.center(), b.width() / 2 + 4, b.height() / 2 + 4);
    }
    p.end();
    res.annotated = annotated;

    return res;
}

bool Detector::isSkinPixel(int r, int g, int b) {
    // 经典 RGB 肤色启发式规则（自然光下有效）
    return r > 95 && g > 40 && b > 20 &&
           (qMax(r, qMax(g, b)) - qMin(r, qMin(g, b))) > 15 &&
           qAbs(r - g) > 15 && r > g && r > b;
}

DetectionResult Detector::runFaceDetection(const QImage& frame) {
    DetectionResult res;
    res.mode = DetectionMode::FaceDetection;
    res.timestampMs = QDateTime::currentMSecsSinceEpoch();
    res.ok = true; // 人脸模式不适用 OK/NG，仅表示检测完成

    if (frame.isNull())
        return res;

    // 1. 缩小加速（肤色检测对分辨率不敏感，先缩到宽<=320）
    QImage small = frame;
    double scale = 1.0;
    if (frame.width() > 320) {
        scale = 320.0 / frame.width();
        small = frame.scaled(int(320), int(frame.height() * scale),
                             Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    if (small.format() != QImage::Format_RGB32)
        small = small.convertToFormat(QImage::Format_RGB32);

    // 2. 肤色二值化
    QImage skin(small.size(), QImage::Format_Grayscale8);
    skin.fill(0);
    const int sw = small.width();
    const int sh = small.height();
    for (int y = 0; y < sh; ++y) {
        const uchar* px = small.constScanLine(y);
        uchar* d = skin.scanLine(y);
        for (int x = 0; x < sw; ++x) {
            const int b = px[x * 4 + 0];
            const int g = px[x * 4 + 1];
            const int r = px[x * 4 + 2];
            d[x] = isSkinPixel(r, g, b) ? 255 : 0;
        }
    }

    // 3. 连通域分析，过滤出人脸候选框
    const auto comps = ImageAlgorithms::connectedComponents(skin, /*minArea=*/80);
    const double inv = 1.0 / scale;
    QPainter p; // 稍后绘制
    QImage annotated = frame.convertToFormat(QImage::Format_RGB32);
    p.begin(&annotated);
    p.setPen(QPen(QColor(0, 220, 0), 3));
    p.setBrush(Qt::NoBrush);

    for (const auto& c : comps) {
        const double ratio = double(c.bounds.width()) / qMax(1, c.bounds.height());
        // 几何过滤：人脸宽高比约 0.4~1.8，面积适中
        if (ratio < 0.4 || ratio > 1.8)
            continue;
        // 映射回原图坐标
        QRect face(qRound(c.bounds.x() * inv), qRound(c.bounds.y() * inv),
                   qRound(c.bounds.width() * inv), qRound(c.bounds.height() * inv));
        // 稍微外扩，框住整张脸
        face.adjust(-face.width() / 8, -face.height() / 5,
                    face.width() / 8, face.height() / 5);
        res.faces.append(face.intersected(QRect(0, 0, frame.width(), frame.height())));
        p.drawRect(face);
    }
    p.end();
    res.annotated = annotated;
    return res;
}

} // namespace vi

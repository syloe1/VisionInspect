#pragma once
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QVector>

namespace vi {

// 检测模式：工业工件缺陷检测 / 人脸检测（用于真实摄像头直观演示）
enum class DetectionMode { WorkInspection, FaceDetection };

// 单个缺陷的信息（连通域分析得到）
struct DefectInfo {
    QPoint pos;    // 缺陷质心
    int area = 0;  // 缺陷面积（像素）
    QRect bounds;  // 缺陷包围盒
};

// 一次检测的完整结果
struct DetectionResult {
    DetectionMode mode = DetectionMode::WorkInspection; // 本次检测模式
    bool ok = true;                 // OK/NG 判定（工件模式）
    bool workFound = false;         // 是否定位到工件
    QRect workArea;                 // 工件包围盒（亮区域）
    int defectCount = 0;            // 缺陷数量
    double defectArea = 0.0;        // 缺陷总面积（像素）
    QVector<DefectInfo> defects;    // 缺陷明细
    QVector<QRect> faces;           // 人脸检测框（人脸模式）
    QImage annotated;               // 叠加标记的结果图
    qint64 timestampMs = 0;         // 检测时刻（毫秒时间戳）
};

} // namespace vi

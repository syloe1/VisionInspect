#pragma once
#include "vision/DetectionResult.h"
#include <QImage>

namespace vi {

// 检测器配置参数（均可在上位机界面中调节）
struct DetectorConfig {
    int workLow = 160;       // 工件亮区域阈值下界（定位工件）
    int defectHigh = 40;     // 缺陷暗区域阈值上界（检测暗缺陷）
    int minWorkArea = 1000;  // 工件最小面积，低于视为未找到
    int minDefectArea = 4;   // 缺陷最小面积，用于过滤噪声
    double maxDefectArea = 200.0; // NG 判定的缺陷面积上限
    int maxDefectCount = 0;       // NG 判定的缺陷数量上限（0=有缺陷即NG）
};

// ------------------------------------------------------------------
// 视觉检测器：支持两种模式
//  - WorkInspection：工业工件定位 + 缺陷 Blob 分析 + OK/NG 判定
//  - FaceDetection：肤色检测定位人脸（真实摄像头直观演示）
// 检测流程（纯 Qt 自写算法，零第三方依赖）：
//   工件：灰度 -> 阈值 -> 8邻域连通域 -> 缺陷统计 -> OK/NG
//   人脸：肤色RGB规则二值化 -> 连通域 -> 几何过滤 -> 人脸框
// ------------------------------------------------------------------
class Detector {
public:
    Detector();
    void setConfig(const DetectorConfig& cfg);
    const DetectorConfig& config() const;

    void setMode(DetectionMode mode) { mode_ = mode; }
    DetectionMode mode() const { return mode_; }

    // 对一帧图像执行检测，返回结构化结果（含叠加标记的结果图）
    DetectionResult detect(const QImage& frame);

private:
    DetectionResult runFaceDetection(const QImage& frame);
    // 肤色像素判定（RGB 启发式规则）
    static bool isSkinPixel(int r, int g, int b);

    DetectorConfig cfg_;
    DetectionMode mode_ = DetectionMode::WorkInspection;
};

} // namespace vi

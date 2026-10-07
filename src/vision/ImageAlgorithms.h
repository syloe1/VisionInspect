#pragma once
#include <QImage>
#include <QRect>
#include <QVector>

namespace vi {

// ------------------------------------------------------------------
// 纯 Qt 实现的图像算法库（零第三方依赖）
// 覆盖机器视觉基础算子：灰度化、阈值分割、连通域分析(Blob)、模板匹配。
// 全部基于 QImage 逐像素处理，便于单测与教学演示。
// ------------------------------------------------------------------

// 灰度连通域分量
struct Component {
    int area = 0;     // 面积（像素数）
    double cx = 0.0;  // 质心 x
    double cy = 0.0;  // 质心 y
    QRect bounds;     // 外接矩形
};

namespace ImageAlgorithms {

// 转为 8 位灰度图（已是灰度则原样返回）
QImage grayscale(const QImage& src);

// 阈值分割：灰度落在 [lo, hi] 的像素置 on，否则置 off，返回二值图
QImage threshold(const QImage& gray, int lo, int hi,
                 uchar on = 255, uchar off = 0);

// 8 邻域连通域分析：统计非零前景区域
// binary 须为 Format_Grayscale8，前景 = 像素值 > 0
// minArea：过滤小于该面积的噪声分量
QVector<Component> connectedComponents(const QImage& binary, int minArea = 0);

// 归一化互相关(NCC)模板匹配：在灰度图中搜索模板，返回最佳分数与位置
// 全图逐窗口计算，适用于小模板/小图（教学演示与测试）
struct MatchResult {
    double score = -1.0;  // 最佳匹配分数 [-1,1]
    int x = 0;            // 匹配左上角 x
    int y = 0;            // 匹配左上角 y
};
MatchResult templateMatch(const QImage& gray, const QImage& tmpl);

} // namespace vi::ImageAlgorithms

} // namespace vi

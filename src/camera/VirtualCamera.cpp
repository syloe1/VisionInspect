#include "camera/VirtualCamera.h"
#include <QImage>
#include <QRandomGenerator>
#include <QThread>
#include <QtMath>

namespace vi {

namespace {
// 限制在 [lo, hi]
inline int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
// 场景几何：工件圆盘
constexpr int kDiskCenterX = 320;
constexpr int kDiskCenterY = 240;
constexpr int kDiskRadius = 110;
} // namespace

VirtualCamera::VirtualCamera(int width, int height)
    : width_(width), height_(height) {}

bool VirtualCamera::open() {
    QMutexLocker locker(&mutex_);
    open_ = true;
    return true;
}

void VirtualCamera::close() {
    QMutexLocker locker(&mutex_);
    open_ = false;
}

bool VirtualCamera::isOpen() const {
    QMutexLocker locker(&mutex_);
    return open_;
}

void VirtualCamera::setDefectRate(double rate) {
    QMutexLocker locker(&mutex_);
    defectRate_ = clamp(int(rate * 1000), 0, 1000) / 1000.0;
}

double VirtualCamera::defectRate() const {
    QMutexLocker locker(&mutex_);
    return defectRate_;
}

void VirtualCamera::setNoise(int noise) {
    QMutexLocker locker(&mutex_);
    noise_ = clamp(noise, 0, 255);
}

int VirtualCamera::noise() const {
    QMutexLocker locker(&mutex_);
    return noise_;
}

QString VirtualCamera::name() const {
    return QStringLiteral("VirtualCamera");
}

bool VirtualCamera::grabFrame(QImage& frame, int timeoutMs) {
    Q_UNUSED(timeoutMs)
    QMutexLocker locker(&mutex_);
    if (!open_)
        return false;

    // 模拟一次采集/传输的短延迟
    QThread::msleep(5);

    const bool withDefects =
        QRandomGenerator::global()->generateDouble() < defectRate_;
    frame = renderFrame(withDefects);
    return true;
}

QImage VirtualCamera::renderFrame(bool withDefects) const {
    QImage img(width_, height_, QImage::Format_Grayscale8);
    img.fill(0);
    auto* rng = QRandomGenerator::global();

    // 逐像素填充：背景灰 60 + 噪声；工件圆盘亮灰 200 + 噪声
    for (int y = 0; y < height_; ++y) {
        uchar* line = img.scanLine(y);
        const int dy = y - kDiskCenterY;
        for (int x = 0; x < width_; ++x) {
            const int dx = x - kDiskCenterX;
            const bool inside =
                (dx * dx + dy * dy) <= (kDiskRadius * kDiskRadius);
            int base = inside ? 200 : 60;
            int v = base + (rng->bounded(2 * noise_ + 1) - noise_);
            line[x] = uchar(clamp(v, 0, 255));
        }
    }

    if (withDefects) {
        // 在工件圆盘内随机生成 1~2 个暗色缺陷斑
        const int n = 1 + rng->bounded(2);
        for (int k = 0; k < n; ++k) {
            const int r = 3 + rng->bounded(3); // 缺陷半径 3~5
            const int a = rng->bounded(360);
            const double rad = qDegreesToRadians(double(a));
            const int dr = 10 + rng->bounded(kDiskRadius - 20);
            const int cx = kDiskCenterX + int(dr * qCos(rad));
            const int cy = kDiskCenterY + int(dr * qSin(rad));

            for (int y = cy - r; y <= cy + r; ++y) {
                if (y < 0 || y >= height_)
                    continue;
                uchar* line = img.scanLine(y);
                for (int x = cx - r; x <= cx + r; ++x) {
                    if (x < 0 || x >= width_)
                        continue;
                    const int dx = x - cx;
                    const int dy = y - cy;
                    if (dx * dx + dy * dy <= r * r)
                        line[x] = 20; // 暗缺陷
                }
            }
        }
    }
    return img;
}

} // namespace vi

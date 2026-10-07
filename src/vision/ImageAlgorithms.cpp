#include "vision/ImageAlgorithms.h"
#include <QQueue>
#include <QtMath>

namespace vi {
namespace ImageAlgorithms {

QImage grayscale(const QImage& src) {
    if (src.isNull())
        return {};
    if (src.format() == QImage::Format_Grayscale8)
        return src.copy();
    return src.convertToFormat(QImage::Format_Grayscale8);
}

QImage threshold(const QImage& gray, int lo, int hi, uchar on, uchar off) {
    if (gray.isNull() || gray.format() != QImage::Format_Grayscale8)
        return {};
    QImage out(gray.size(), QImage::Format_Grayscale8);
    const int w = gray.width();
    const int h = gray.height();
    for (int y = 0; y < h; ++y) {
        const uchar* s = gray.constScanLine(y);
        uchar* d = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const uchar v = s[x];
            d[x] = (v >= lo && v <= hi) ? on : off;
        }
    }
    return out;
}

QVector<Component> connectedComponents(const QImage& binary, int minArea) {
    QVector<Component> comps;
    if (binary.isNull() || binary.format() != QImage::Format_Grayscale8)
        return comps;

    const int w = binary.width();
    const int h = binary.height();
    if (w == 0 || h == 0)
        return comps;

    QVector<uchar> visited(size_t(w) * size_t(h), 0);

    // 8 邻域偏移
    static const int dx8[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const int dy8[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int y = 0; y < h; ++y) {
        const uchar* row = binary.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const size_t idx = size_t(y) * w + x;
            if (visited[idx] || row[x] == 0)
                continue;

            // BFS 收集一个连通域
            QQueue<int> qx, qy;
            qx.enqueue(x);
            qy.enqueue(y);
            visited[idx] = 1;

            int area = 0;
            qint64 sumX = 0, sumY = 0;
            int minX = x, maxX = x, minY = y, maxY = y;

            while (!qx.isEmpty()) {
                const int cx = qx.dequeue();
                const int cy = qy.dequeue();
                ++area;
                sumX += cx;
                sumY += cy;
                if (cx < minX) minX = cx;
                if (cx > maxX) maxX = cx;
                if (cy < minY) minY = cy;
                if (cy > maxY) maxY = cy;

                const uchar* crow = binary.constScanLine(cy);
                for (int k = 0; k < 8; ++k) {
                    const int nx = cx + dx8[k];
                    const int ny = cy + dy8[k];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    const size_t nidx = size_t(ny) * w + nx;
                    if (!visited[nidx] && binary.constScanLine(ny)[nx] != 0) {
                        visited[nidx] = 1;
                        qx.enqueue(nx);
                        qy.enqueue(ny);
                    }
                }
            }

            if (area >= minArea) {
                Component c;
                c.area = area;
                c.cx = double(sumX) / area;
                c.cy = double(sumY) / area;
                c.bounds = QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
                comps.append(c);
            }
        }
    }
    return comps;
}

MatchResult templateMatch(const QImage& gray, const QImage& tmpl) {
    MatchResult best;
    if (gray.isNull() || tmpl.isNull() ||
        gray.format() != QImage::Format_Grayscale8 ||
        tmpl.format() != QImage::Format_Grayscale8)
        return best;
    if (tmpl.width() > gray.width() || tmpl.height() > gray.height())
        return best;

    const int gw = gray.width(), gh = gray.height();
    const int tw = tmpl.width(), th = tmpl.height();

    // 模板均值
    double tmplMean = 0.0;
    for (int y = 0; y < th; ++y) {
        const uchar* s = tmpl.constScanLine(y);
        for (int x = 0; x < tw; ++x)
            tmplMean += s[x];
    }
    tmplMean /= (tw * th);

    // 模板去均值
    QVector<double> t(tw * th);
    double tNorm = 0.0;
    for (int y = 0; y < th; ++y) {
        const uchar* s = tmpl.constScanLine(y);
        for (int x = 0; x < tw; ++x) {
            double v = s[x] - tmplMean;
            t[size_t(y) * tw + x] = v;
            tNorm += v * v;
        }
    }
    if (tNorm <= 0.0)
        return best;

    const int maxY = gh - th;
    const int maxX = gw - tw;
    for (int oy = 0; oy <= maxY; ++oy) {
        for (int ox = 0; ox <= maxX; ++ox) {
            double winMean = 0.0;
            for (int y = 0; y < th; ++y) {
                const uchar* s = gray.constScanLine(oy + y);
                for (int x = 0; x < tw; ++x)
                    winMean += s[ox + x];
            }
            winMean /= (tw * th);

            double num = 0.0, denomB = 0.0;
            for (int y = 0; y < th; ++y) {
                const uchar* s = gray.constScanLine(oy + y);
                for (int x = 0; x < tw; ++x) {
                    const double wv = s[ox + x] - winMean;
                    const double tv = t[size_t(y) * tw + x];
                    num += wv * tv;
                    denomB += wv * wv;
                }
            }
            if (denomB <= 0.0)
                continue;
            const double score = num / qSqrt(tNorm * denomB);
            if (score > best.score) {
                best.score = score;
                best.x = ox;
                best.y = oy;
            }
        }
    }
    return best;
}

} // namespace ImageAlgorithms
} // namespace vi

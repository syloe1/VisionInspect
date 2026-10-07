// 摄像头冒烟测试：验证 MediaFoundationCamera 能打开真实摄像头并抓到帧
// 用途：交付前自检（依赖本机摄像头，不作为常规单测注册）
#include <QCoreApplication>
#include <QDebug>
#include "camera/MediaFoundationCamera.h"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    vi::MediaFoundationCamera cam;
    if (!cam.open()) {
        qWarning().noquote() << "CAM_OPEN_FAILED";
        return 1;
    }
    qInfo().noquote() << "CAM_OPENED" << cam.name();
    QImage frame;
    bool got = false;
    for (int i = 0; i < 15 && !got; ++i) {
        if (cam.grabFrame(frame) && !frame.isNull()) {
            got = true;
            qInfo().noquote() << "FRAME_OK" << frame.width() << "x" << frame.height()
                              << "format" << frame.format();
        }
    }
    cam.close();
    qInfo().noquote() << (got ? "SMOKE_PASS" : "SMOKE_FAIL");
    return got ? 0 : 2;
}

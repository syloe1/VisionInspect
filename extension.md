# VisionInspect 扩展指南（extension.md）

> 每个扩展给出：**目标 / 涉及文件 / 实现思路 / 关键代码骨架**。
> ✅ 表示**已在当前项目实现**，可直接运行；其余为待扩展。
> 按投入产出比排序，建议优先做 E2、E3、E6（改动小、最加分）。

---

## 扩展总览

| 编号 | 扩展 | 状态 | 对应岗位价值 |
|---|---|---|---|
| E1 | 真实相机（USB/工业 SDK） | ✅ 已实现 Media Foundation 真实摄像头 | 相机 SDK、软硬件联调 ⭐ |
| E4 | 新增视觉算法 | ✅ 已实现人脸检测（肤色 Blob） | 机器视觉 ⭐ |
| E2 | 图像保存 / 回放 | 待扩展 | 现场调试、问题回溯 ⭐ |
| E3 | 参数持久化（QSettings） | 低 | 工程化 ⭐ |
| E4 | 新增视觉算法 | 中 | 机器视觉 ⭐ |
| E5 | 串口通信（QSerialPort） | 中 | 硬件通信 |
| E6 | TCP 模拟服务端（本地联调） | 低 | 网络编程、联调 ⭐ |
| E7 | 多相机 / 多流水线 | 高 | 架构设计 |
| E8 | 结果入库（SQLite） | 中 | 数据管理 |
| E9 | 扩展单元测试 | 低 | 测试意识 |
| E10 | 打包发布（windeployqt） | 低 | 交付能力 |
| E11 | 运行日志落盘 | 低 | 现场排障 |
| E12 | 性能优化 | 中 | 实时性 |

---

## E1 接入真实相机（✅ 已实现 Media Foundation）

**现状**：本机 USB 摄像头已实现为 **`MediaFoundationCamera`**（`src/camera/`），用 Windows 原生 Media Foundation 采集，输出 **1280x720 RGB32**，界面相机下拉框可选。已通过 `tests/camera_smoke.cpp` 冒烟验证（`FRAME_OK 1280x720 RGB32`）。

> 注：本机 Qt 未装 Multimedia 模块，故未用 `QCamera`，改用 Media Foundation（MinGW 自带导入库，零第三方依赖）。

**仍可扩展：接入工业相机 SDK**（海康/巴斯勒/Halcon）：新增 `src/camera/IndustrialCamera.h/.cpp` 实现 `CameraInterface`，在相机 SDK 采集回调里把图像转 `QImage`，再在 `MainWindow::startPipeline` 的下拉分支里切换即可。

**参考实现（Media Foundation 核心流程）**：

```cpp
// MediaFoundationCamera.cpp —— 已实现，核心步骤
// 1. 枚举设备并激活
MFCreateAttributes(&attrs, 1);
attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
               MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
MFEnumDeviceSources(attrs, &devices, &count);
devices[0]->ActivateObject(IID_PPV_ARGS(&source_));

// 2. 创建 SourceReader，启用视频处理以便转 RGB32
MFCreateAttributes(&rdAttrs, 1);
rdAttrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
MFCreateSourceReaderFromMediaSource(source_, rdAttrs, &reader_);

// 3. 请求 RGB32 输出子类型
mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
mt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
reader_->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mt);

// 4. 同步读帧，拷贝进 QImage（深拷贝，脱离 MF 缓冲区）
reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &idx, &flags, &ts, &sample);
// ... ConvertToContiguousBuffer -> Lock -> QImage(data,w,h,stride,RGB32).copy()
```

**CMake**：链接 Windows 导入库 `mfplat mfreadwrite mfuuid mf`（已加）。

**注意**：采集线程首次调用 `grabFrame` 需初始化 COM（`CoInitializeEx`，幂等）。

---

## E2 图像保存 / 回放

**目标**：NG 帧自动存图、按路径回放。

**涉及文件**：`src/ui/MainWindow.cpp`，可在 `onResult` 里保存；新增一个回放线程或直接用 `QTimer` 逐帧播放。

**代码骨架（NG 自动存图）**：

```cpp
// 在 onResult 中，判 NG 时保存
if (!result.ok) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                        + "/VisionInspect/NG";
    QDir().mkpath(dir);
    const QString name = QString("NG_%1.png")
        .arg(QDateTime::currentMSecsSinceEpoch());
    result.annotated.save(dir + "/" + name);   // 保存带标记的结果图
}
```

**回放**：把保存的图片路径列表读进来，用 `QTimer` 每 100ms 发一帧进 `onFrame`（走同一显示通道）。

---

## E3 参数持久化（QSettings）

**目标**：重启后记住检测参数和 TCP 地址端口。

**涉及文件**：`src/ui/MainWindow.cpp`（读写），`applyDetectorParams`、`connectTcp`、构造函数。

**代码骨架**：

```cpp
#include <QSettings>

void MainWindow::saveSettings() {
    QSettings s(QStringLiteral("VisionInspect"), QStringLiteral("config"));
    s.setValue("detect/workLow", workLowSpin_->value());
    s.setValue("detect/defectHigh", defectHighSpin_->value());
    s.setValue("tcp/host", hostEdit_->text());
    s.setValue("tcp/port", portSpin_->value());
}
void MainWindow::loadSettings() {
    QSettings s(QStringLiteral("VisionInspect"), QStringLiteral("config"));
    workLowSpin_->setValue(s.value("detect/workLow", 160).toInt());
    defectHighSpin_->setValue(s.value("detect/defectHigh", 40).toInt());
    hostEdit_->setText(s.value("tcp/host", "127.0.0.1").toString());
    portSpin_->setValue(s.value("tcp/port", 8000).toInt());
}
```

在构造函数末尾调 `loadSettings()`，在"应用参数"和连接 TCP 时调 `saveSettings()`。

---

## E4 新增视觉算法

**✅ 已实现：人脸检测（肤色 Blob）** —— `Detector` 新增 `FaceDetection` 模式，纯 Qt 实现：缩小加速 → 肤色 RGB 规则二值化 → 8 邻域连通域 → 几何过滤 → 人脸框。对应真实摄像头演示，已配单测 `faceDetectionTest`。

**目标（可继续扩展）**：给 `ImageAlgorithms` 增加边缘检测（Canny）和直方图统计，或把已有的 `templateMatch` 实装进检测流程。

**涉及文件**：`src/vision/ImageAlgorithms.h/.cpp`、`src/vision/Detector.cpp`、`tests/test_vision.cpp`。

**思路**：保持纯函数风格（输入 QImage → 输出 QImage/数值），不碰 UI 和线程，方便单测。

**代码骨架（Sobel 边缘，返回梯度幅值图）**：

```cpp
// ImageAlgorithms.h
QImage sobelEdges(const QImage& gray);

// ImageAlgorithms.cpp —— 用 scanLine 逐行访问（勿用 constBits 线性索引！）
QImage ImageAlgorithms::sobelEdges(const QImage& gray) {
    QImage out(gray.size(), QImage::Format_Grayscale8);
    for (int y = 1; y < gray.height() - 1; ++y) {
        const uchar* p0 = gray.constScanLine(y - 1);
        const uchar* p1 = gray.constScanLine(y);
        const uchar* p2 = gray.constScanLine(y + 1);
        uchar* d = out.scanLine(y);
        for (int x = 1; x < gray.width() - 1; ++x) {
            int gx = p0[x-1] + 2*p0[x] + p0[x+1] - p2[x-1] - 2*p2[x] - p2[x+1];
            int gy = p0[x-1] + 2*p1[x-1] + p2[x-1] - p0[x+1] - 2*p1[x+1] - p2[x+1];
            d[x] = uchar(qMin(255, int(qSqrt(gx*gx + gy*gy))));
        }
    }
    return out;
}
```

**接入**：可在检测前对 ROI 做边缘增强，或在界面加一个"边缘视图"开关，把 `sobelEdges` 结果发到显示通道。

---

## E5 串口通信（QSerialPort）

**目标**：与 PLC/下位机走串口收发（JD 的"硬件通信调试"）。

**涉及文件**：新增 `src/comm/SerialComm.h/.cpp`；CMake 的 `find_package` 加 `SerialPort`，`target_link_libraries` 加 `Qt6::SerialPort`。

**代码骨架**：

```cpp
// 头文件
#include <QSerialPort>
class SerialComm : public QObject {
    Q_OBJECT
public:
    bool open(const QString& portName, qint32 baud = 115200) {
        if (serial_.isOpen()) serial_.close();
        serial_.setPortName(portName);
        serial_.setBaudRate(baud);
        serial_.setDataBits(QSerialPort::Data8);
        serial_.setParity(QSerialPort::NoParity);
        serial_.setStopBits(QSerialPort::OneStop);
        return serial_.open(QIODevice::ReadWrite);
    }
    void close() { serial_.close(); }
    qint64 send(const QByteArray& data) { return serial_.write(data); }
signals:
    void dataReceived(const QByteArray& data);
private:
    QSerialPort serial_;
};
```

**对接**：可在 `onResult` 里同时发串口一条简单报文（如 `OK` / `NG` 一个字节 0x01/0x00），模拟通知 PLC 分拣。

---

## E6 TCP 模拟服务端（本地联调）

**目标**：写一个小服务端接收上报，验证协议。

**涉及文件**：新建独立小程序或脚本（不在主程序内）。可用 Qt 或 Python。

**思路**：监听端口，读帧，按 `0xAA 0x55 | len | JSON | crc` 解帧、校验 CRC、打印结果。

**代码骨架（Python，最简单）**：

```python
import socket, struct

def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

srv = socket.socket(); srv.bind(("127.0.0.1", 8000)); srv.listen(1)
conn, _ = srv.accept()
print("client connected")
while True:
    hdr = conn.recv(6)                       # 帧头2 + 长度4
    if len(hdr) < 6: break
    assert hdr[:2] == b"\xAA\x55"
    n = struct.unpack("<I", hdr[2:6])[0]
    payload = b""
    while len(payload) < n:
        payload += conn.recv(n - len(payload))
    crc = struct.unpack("<H", conn.recv(2))[0]
    ok = crc16(hdr[2:] + payload) == crc
    print(f"CRC={'OK' if ok else 'FAIL'} payload={payload.decode()}")
```

---

## E7 多相机 / 多流水线

**目标**：同时采集多路相机、各自独立检测。

**思路**：把"一个 Pipeline 内部一个队列 + 一组线程"抽象成"每路一个 Pipeline 实例"，`MainWindow` 持有 `QVector<std::shared_ptr<Pipeline>>`，在界面用 Tab 切换各路画面。

**涉及文件**：`MainWindow` 改为管理多个 `Pipeline`；每个 Pipeline 已有独立的队列与线程，天然隔离，只需把各路 `resultReady` 路由到对应 Tab。

**注意**：线程数量 = 相机数 × 2，注意线程栈与 CPU 占用；可考虑后续用线程池优化。

---

## E8 结果入库（SQLite）

**目标**：把每次检测结果存进本地数据库，便于统计与追溯。

**涉及文件**：新增 `src/comm/ResultDb.h/.cpp`；CMake 加 `Sql`。

**代码骨架**：

```cpp
#include <QSqlDatabase>
#include <QSqlQuery>

bool ResultDb::init(const QString& path) {
    db_ = QSqlDatabase::addDatabase("QSQLITE");
    db_.setDatabaseName(path);
    if (!db_.open()) return false;
    QSqlQuery q(db_);
    q.exec("CREATE TABLE IF NOT EXISTS results("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "ts INTEGER, ok INTEGER, defect_count INTEGER, defect_area REAL,"
           "work_x INTEGER, work_y INTEGER, work_w INTEGER, work_h INTEGER)");
    return true;
}
void ResultDb::insert(const DetectionResult& r) {
    QSqlQuery q(db_);
    q.prepare("INSERT INTO results(ts,ok,defect_count,defect_area,work_x,work_y,work_w,work_h)"
              " VALUES(?,?,?,?,?,?,?,?)");
    q.addBindValue(r.timestampMs); q.addBindValue(r.ok ? 1 : 0);
    q.addBindValue(r.defectCount); q.addBindValue(r.defectArea);
    q.addBindValue(r.workArea.x()); q.addBindValue(r.workArea.y());
    q.addBindValue(r.workArea.width()); q.addBindValue(r.workArea.height());
    q.exec();
}
```

在 `onResult` 里调 `db_.insert(result)`。注意 `QSqlDatabase::addDatabase` 有全局命名，多实例需指定连接名。

---

## E9 扩展单元测试

**目标**：给新增算法/模块补测试，保持"改完可验证"。

**涉及文件**：`tests/test_vision.cpp`（仿照现有 `detectorOkTest` / `connectedComponentsTest`）。

**示例（给 E4 的 Sobel 写测试）**：

```cpp
void TestVision::sobelTest() {
    QImage gray(5,5, QImage::Format_Grayscale8);
    gray.fill(0);
    // 左半暗、右半亮，构造竖直边缘
    for (int y=0;y<5;++y) for (int x=0;x<2;++x) gray.setPixelColor(x,y,QColor(0,0,0));
    for (int y=0;y<5;++y) for (int x=2;x<5;++x) gray.setPixelColor(x,y,QColor(200,200,200));
    QImage e = ImageAlgorithms::sobelEdges(gray);
    // 边缘列(第2列)梯度应明显大于平坦区
    QVERIFY(e.pixelColor(2,2).red() > e.pixelColor(0,2).red());
}
```

---

## E10 打包发布

**目标**：让别的电脑也能直接运行（绿色版或安装包）。

**涉及文件**：构建产物；用 Qt 自带工具。

**步骤**：

```bash
# 1. 部署依赖 DLL 到 exe 同目录
D:/Qt/6.11.1/mingw_64/bin/windeployqt.exe build/src/VisionInspect.exe

# 2. 拷贝 exe 和生成的文件一起发给用户即可（绿色版）
# 3. 进阶：用 NSIS / Inno Setup 打安装包
```

**注意**：`windeployqt` 会复制 Qt 运行库、插件（platforms、imageformats 等），务必带 `platforms/qwindows.dll`，否则目标机器无法启动 GUI。

---

## E11 运行日志落盘

**目标**：界面日志同时写文件，方便现场排障。

**涉及文件**：`src/ui/MainWindow.cpp` 的 `appendLog`，或用 `qInstallMessageHandler` 全局接管。

**代码骨架（appendLog 同时写文件）**：

```cpp
#include <QFile>
#include <QTextStream>

void MainWindow::appendLog(const QString& msg) {
    logView_->appendPlainText(QDateTime::currentDateTime()
        .toString(QStringLiteral("HH:mm:ss.zzz ")) + msg);
    static QFile f(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/visioninspect.log");
    if (!f.isOpen()) { QDir().mkpath(QFileInfo(f).absolutePath()); f.open(QIODevice::Append); }
    QTextStream(&f) << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                    << " " << msg << "\n";
}
```

---

## E12 性能优化

**目标**：提高实时性，降低延迟。

**思路（按收益排序）**：

1. **图像缩放显示**：`onFrame` 里 `scaled` 到 label 尺寸再显示，别塞原图大图（本项目已做）。
2. **算法用 `scanLine` + 局部变量**：避免逐像素 `pixelColor`（慢几十倍）。
3. **降低噪声渲染成本**：`VirtualCamera::renderFrame` 的逐像素随机数可改为"每行种子 + 少量随机"，或缩小图像。
4. **多路并行**：E7 多相机时用 `QThreadPool` + `QtConcurrent::run` 并行处理各路。
5. **队列容量调优**：实测延迟与丢帧，在 `FrameQueue(4)` 附近找平衡。
6. **`Detector` 加耗时测量**：用 `QElapsedTimer` 打点，量化每步耗时，再针对性优化。

**验证**：用状态栏 FPS 和新增的"检测耗时"日志对比优化前后。

---

> **建议路线**：先做 E1 + E2 + E3 + E6 这 4 个（改动小、最贴近岗位），再挑 E4 加一个自己的算法。做完后在简历上写：**"在可插拔相机架构上接入真实相机、实现 NG 自动存图与参数持久化，并自建 TCP 服务端完成协议联调"** —— 这就是很扎实的一段经历。

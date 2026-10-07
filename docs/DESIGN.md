# VisionInspect 软件设计说明

版本：1.1.0
日期：2026-10-07

---

## 1. 概述

VisionInspect 是一套工业视觉检测上位机软件的演示实现，覆盖从相机采集、图像处理、缺陷检测到结果上报的完整链路。系统采用 **UI 层 / 业务调度层 / 硬件抽象层** 三层架构，通过**生产者-消费者多线程流水线**保证实时性。支持**虚拟相机**与**真实摄像头（Media Foundation）**两种采集源，并提供**工件缺陷检测**与**人脸检测**两种算法模式。

### 1.1 设计目标
- **可扩展**：相机、检测算法、通信模块均可插拔替换。
- **实时**：有界队列 + 多线程，保证处理端始终消费最新帧。
- **可测试**：核心逻辑与 GUI 解耦，纯 Qt 算法可独立单测。
- **可交付**：跨平台（Windows/Linux）、CMake 一键构建。

---

## 2. 总体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        UI 层  (MainWindow)                       │
│  实时画面显示 · 检测参数面板 · 结果统计 · 日志 · TCP 配置           │
└───────────────┬─────────────────────────────────────────────────┘
                │ Qt 信号/槽（跨线程自动排队）
┌───────────────▼─────────────────────────────────────────────────┐
│                    业务调度层  (Pipeline)                        │
│  组装相机 + 队列 + 采集/处理线程 · FPS 统计 · 启停控制            │
└───────┬───────────────────────────────┬─────────────────────────┘
        │ grabFrame                     │ 帧
┌───────▼────────┐   push/pop   ┌───────▼─────────────────────────┐
│  硬件抽象层      │◄────────────►│  视觉处理层                      │
│  CameraInterface│  FrameQueue  │  ImageAlgorithms + Detector     │
│  ├VirtualCamera │              │  (灰度/阈值/连通域/模板匹配)      │
│  └MediaFoundation│             │  ├工件缺陷检测(OK/NG)            │
│    Camera(真实)  │              │  └人脸检测(肤色Blob)             │
└───────▲────────┘               └───────────────┬─────────────────┘
        │                                         ▼
        │                              ┌───────────────────────────┐
        │                              │  通信层  TcpReporter       │
        │                              │  (帧头+长度+JSON+CRC16)     │
        └──────────────────────────────┴───────────┬───────────────┘
                                                    │ 上报 OK/NG 给 PLC/服务器
```

### 2.1 模块划分

| 模块 | 目录 | 职责 |
|---|---|---|
| camera | `src/camera` | 相机抽象接口、虚拟相机、Media Foundation 真实相机 |
| core | `src/core` | 有界帧队列、采集/处理线程、流水线调度 |
| vision | `src/vision` | 图像算法库、检测器(工件/人脸)、检测结果结构 |
| comm | `src/comm` | TCP 结果上报 |
| ui | `src/ui` | 上位机界面 |

---

## 3. 详细设计

### 3.1 相机抽象层（camera）

```cpp
class CameraInterface {
public:
    virtual ~CameraInterface() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool grabFrame(QImage& frame, int timeoutMs = 1000) = 0;
    virtual QString name() const = 0;
};
```

- **VirtualCamera**：无硬件时的模拟源，每帧实时渲染"背景 + 亮色圆盘工件 + 随机暗缺陷"，叠加噪声模拟真实采集。`defectRate` 控制出缺陷概率。
- **MediaFoundationCamera**：真实摄像头，基于 Windows Media Foundation（`IMFSourceReader`）枚举设备、请求 RGB32、读取帧；用 `MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING` 启用格式转换。采集线程首次调用时初始化 COM。
- **接入工业相机**：新增实现类调用工业相机 SDK 的采集接口，通过 `Pipeline::setCamera` 注入即可，上层零改动（依赖倒置 / 开闭原则）。

### 3.2 多线程流水线（core）

**有界帧队列 FrameQueue**（生产者-消费者）：
- `push`：队满时**丢弃最旧帧**再压入，天然实现背压与实时性。
- `pop`：空则等待，支持超时，避免死等。
- 内部 `QMutex + QWaitCondition`，线程安全。

**线程划分**：

| 线程 | 角色 | 循环 |
|---|---|---|
| `AcquisitionThread` | 生产者 | `camera->grabFrame` → `queue.push`，并发出 `frameCaptured` |
| `ProcessingThread` | 消费者 | `queue.pop` → `detector.detect` → 发出 `resultReady` |
| UI 主线程 | 展示 | 通过信号槽接收帧与结果，跨线程自动排队 |

**Pipeline**：组装以上组件，提供 `start/stop/setCamera`；用 `QTimer` 每秒统计 FPS。

### 3.3 视觉算法层（vision）

纯 Qt 自写，`QImage` 逐像素处理：

| 函数 | 算法 |
|---|---|
| `grayscale` | 转 8 位灰度图 |
| `threshold` | 阈值分割（区间 [lo,hi] → on/off） |
| `connectedComponents` | **8 邻域 BFS** 连通域分析，输出面积/质心/包围盒 |
| `templateMatch` | 归一化互相关（NCC）模板匹配 |

**检测流程（Detector）**：`DetectionMode` 决定走哪条算法分支。

```
工件模式(WorkInspection):
  灰度化 → 阈值提取亮区(workLow) → 8邻域连通域取最大=工件
       → 工件包围盒内阈值提取暗缺陷(defectHigh) → 连通域统计缺陷
       → OK/NG 判定（缺陷数/面积阈值）→ 生成叠加标记结果图

人脸模式(FaceDetection):
  缩小加速(宽≤320) → 肤色RGB规则二值化 → 8邻域连通域
       → 几何过滤(宽高比/面积) → 生成人脸框标记图
```

### 3.4 通信层（comm / TcpReporter）

自定义帧协议，覆盖网络编程与硬件通信调试：

```
┌──────────┬──────────────┬────────────────────┬────────┐
│ 帧头2字节 │ 载荷长度4字节 │   载荷 payload      │ CRC16  │
│ 0xAA 0x55│  uint32 LE   │  JSON (紧凑)        │ uint16 │
└──────────┴──────────────┴────────────────────┴────────┘
```

- CRC16 覆盖「长度 + 载荷」，可检测传输错误。
- 支持断线自动重连（3 秒定时器）。
- 帧组包与 CRC 为静态方法，便于单测。

### 3.5 界面层（ui / MainWindow）

- 中央实时画面（`QLabel` + 缩放显示，显示检测标记图）。
- 右侧控制面板：**检测模式（工件/人脸）**、检测参数、TCP 配置、结果统计（OK/NG/总数/良率，人脸模式下显示检出人数）。
- 底部日志窗口；状态栏显示 FPS 与图像尺寸。

---

## 4. 关键设计决策

| 决策 | 理由 |
|---|---|
| 依赖倒置相机接口 | 真实相机接入零改动，符合开闭原则 |
| Media Foundation 采集 | 用 Windows 原生 API，零第三方依赖即可用本机摄像头 |
| 有界队列丢旧帧 | 实时检测场景优先新帧，避免积压延迟 |
| 纯 Qt 自写算法 | 零第三方依赖、可单测、可讲解实现原理 |
| 线程间信号槽 | 解耦且自动跨线程排队，避免手写锁同步 UI |
| 协议独立静态方法 | 便于单测与软硬件联调 |

## 5. 测试

见 `tests/test_vision.cpp`，Qt Test 共 11 项，覆盖：
灰度、阈值、连通域、检测器（工件 OK/NG / 未找到工件 / **人脸检出**）、帧队列、TCP 协议（JSON 字段 + 帧结构 + CRC）。
另有 `tests/camera_smoke.cpp` 摄像头冒烟测试（验证真实采集）。
当前结果：**11 passed / 0 failed**。

# VisionInspect 学习指南（learn.md）

> 这是「怎么把这个现成项目**学懂、学透、能讲、能改**」的行动地图，不是 Qt 入门教程。
> 核心思路：**先跑起来 → 按数据流读一遍 → 动手改验证 → 深挖概念 → 面试化**。

---

## 0. 先定目标（一个月）

| 目标 | 验收标准 |
|---|---|
| 看得懂 | 不看代码能画出三层架构和数据流 |
| 改得动 | 能独立完成第 3 部分的"小作业" |
| 讲得出 | 对面试官完整讲 5 分钟 + 答出高频追问 |
| 有增量 | 至少完成 extension.md 里的 2 个扩展 |

**心态**：你是在已有 50 分的基础上冲 75 分，不是从零学 Qt。

---

## 1. 学习顺序总览

```
第1周  跑起来 + 按数据流通读全代码（第2、3部分）
第2周  动手改 + 深挖4个核心概念（第4、5部分）
第3周  做扩展 + 补Qt知识盲区（extension.md）
第4周  面试话术 + 简历整理（第6部分）
```

---

## 2. 第一遍：先让它在脑子里"跑"起来

1. **构建并运行**（见 README），点「开始采集」，观察画面、统计、日志。
2. **界面 ↔ 代码对应**：打开右侧面板每个控件，回 MainWindow.cpp 找它对应的成员变量。
3. **画出数据流**（用纸或笔记）：

```
main → MainWindow(UI)
        │ 点击「开始采集」
        ▼
Pipeline::start()
        │ 打开 VirtualCamera
        ▼
┌─ AcquisitionThread(采集线程) ─┐
│   camera->grabFrame()        │  ──push──►  FrameQueue(有界队列)
└─────────────────────────────┘
┌─ ProcessingThread(处理线程) ─┐
│   queue.pop() ─► Detector    │  ──emit resultReady──►  MainWindow(UI线程)
└─────────────────────────────┘        │
                                       ├─ 统计 OK/NG
                                       ├─ 显示标记图
                                       └─ TcpReporter.sendResult() 上报
```

---

## 3. 第二遍：按数据流读代码（主线）

沿"采集→传输→处理→显示→上报"一条线走，每一站给出**读什么文件**和**验证理解的 checkpoint**。

### 站 1：入口 `src/main.cpp`
- 创建 `QApplication`、`MainWindow`、`exec()` 进入事件循环。
- **Checkpoint**：为什么 `QApplication` 必须比 `MainWindow` 先创建？（答：管理 GUI 资源与事件循环，先于任何 QWidget。）

### 站 2：UI 层 `src/ui/MainWindow.h/.cpp`
- 看 `buildUi()`：如何拼装工具栏/画面/右侧面板/日志。
- 看信号槽连接：`connect(&pipeline_, &Pipeline::frameReady, this, &MainWindow::onFrame)`。
- **Checkpoint**：`onFrame` / `onResult` 运行在哪个线程？（答：UI 主线程，因为信号槽跨线程自动排队。）

### 站 3：调度器 `src/core/Pipeline.h/.cpp`
- `start()` 做了几件事：开相机→清队列→建线程→连信号→启动→起 FPS 定时器。
- 注意 `QTimer` 每秒触发 `onFpsTick` 统计 FPS。
- **Checkpoint**：为什么 FPS 用定时器统计而不是线程内计数？（答：避免在采集线程加锁/计时影响采集，把统计工作交给主线程。）

### 站 4：采集线程 + 相机 `AcquisitionThread.cpp`、`camera/`
- `VirtualCamera::grabFrame` 每帧 `renderFrame` 渲染一幅场景图。
- **Checkpoint**：`CameraInterface` 为什么设计成纯虚接口？（答：依赖倒置——上层只认接口，换相机零改动。）

### 站 5：有界队列 `src/core/FrameQueue.h/.cpp`
- 读 `push` / `pop`，注意 `QMutex + QWaitCondition`，队满**丢最旧帧**。
- **Checkpoint**：队满为什么不阻塞生产者而选择丢帧？（答：实时场景优先新帧，避免积压导致延迟越来越大。）

### 站 6：处理线程 + 检测器 `ProcessingThread.cpp`、`src/vision/`
- `ProcessingThread::run` 循环 `queue.pop → detector.detect → emit resultReady`。
- 读 `Detector::detect` 的 5 步流程（灰度→阈值→连通域→缺陷统计→判定）。
- **Checkpoint**：`Detector` 为什么不含 UI、不含线程？（答：纯算法，职责单一，便于单测。）

### 站 7：通信 `src/comm/TcpReporter.h/.cpp`
- 读帧结构：`0xAA 0x55 | 长度4字节 | JSON | CRC16`。
- 看 `buildPacket` / `crc16` 是静态方法。
- **Checkpoint**：为什么组包和 CRC 做成静态方法？（答：不依赖 socket 状态，可单独单元测试、可复用。）

**通读完**：合上代码，在白纸上把上面 7 站的数据流默写出来，卡住的地方就是没真懂，回头重读。

---

## 4. 第三遍：动手改（验证真懂）

每个小作业先自己写，再看答案思路；**跑通了才算会**。

| # | 作业 | 涉及文件 | 预期 |
|---|---|---|---|
| 1 | 把背景噪声改大/改小 | VirtualCamera.cpp | 画面更花/更干净 |
| 2 | 让虚拟相机 100% 出缺陷 | VirtualCamera.h `defectRate_` | 全是 NG |
| 3 | 界面加一个"缺陷率"显示 | MainWindow.cpp | 实时显示 |
| 4 | 把 FPS 统计改成每 2 秒一次 | Pipeline.cpp | 数字更新变慢 |
| 5 | 日志加一行"检测耗时(ms)" | Detector.cpp + MainWindow | 日志出现耗时 |
| 6 | 协议里加一个"产品号"字段 | TcpReporter.cpp | JSON 多一个字段 |
| 7 | 新增一个统计项"连续NG次数" | MainWindow.cpp | 面板多一栏 |

---

## 5. 深挖四个核心概念（面试必问）

### 5.1 信号槽
- 读 `connect` 的连接方式：同线程默认 `DirectConnection`，跨线程默认 `QueuedConnection`。
- **为什么本项目跨线程通信安全**：`frameCaptured` 在采集线程 emit，槽 `onFrame` 在主线程执行，Qt 自动把参数拷贝排队，无需手写锁。
- 追问：`Qt::QueuedConnection` 的参数为什么要求可拷贝构造？

### 5.2 线程模型
- 回答："我有三个线程：采集线程只做 grabFrame 并 push；处理线程 pop 并 detect；UI 线程只接收结果更新界面。三者通过**有界队列 + 信号槽**解耦。"
- 追问：为什么不让 UI 线程直接采集？（答：采集会阻塞事件循环，界面卡死；且处理耗时要与采集并行。）

### 5.3 背压与实时性
- 回答："队列容量固定，满时丢最旧帧，保证处理端永远消费最新数据，延迟不会累积。"
- 追问：如果处理比采集慢怎么办？（答：丢帧，fps 会下降，实时性优先于完整性。）

### 5.4 依赖倒置（相机抽象）
- 回答："上位机依赖 `CameraInterface` 抽象而不是具体相机。接真实相机就是新增一个实现类，通过 `setCamera` 注入，零改动上层。"
- 追问：怎么保证新增相机不会破坏现有逻辑？（答：接口不变，实现被注入，符合开闭原则；也可加集成测试。）

---

## 6. 面试怎么讲

### 30 秒电梯版
> "我用 Qt/C++ 做了一个工业视觉检测上位机：可插拔相机接口 + 多线程采集处理流水线 + 纯 Qt 自写缺陷检测算法 + TCP 结果上报，覆盖了上位机的完整链路。"

### 2 分钟详细版
> 按数据流讲：相机抽象（为什么可插拔）→ 采集线程进有界队列（为什么满丢帧）→ 处理线程跑检测（灰度/阈值/连通域）→ 结果回 UI（跨线程信号槽）→ TCP 上报（协议+CRC）。每段一句"这里体现了 XX 工程思想"。

### 高频追问清单
1. 信号槽原理和跨线程安全？
2. 多线程你怎么解耦的？
3. 队列满怎么办，为什么？
4. 相机接口为什么这么设计？
5. 实时性怎么保证？
6. 你怎么测试的？（答：Qt Test 10 项，覆盖算法/队列/协议）
7. 接入真实相机你怎么做？（答：新增 CameraInterface 实现）

---

## 7. 常见坑（提前避开）

- **别改完不编译**：每次改完立即 `cmake --build`，别攒一堆。
- **别在槽里做耗时操作**：`onResult` 里只做 UI 更新和发 TCP，别放重型算法。
- **别直接 `wait()` 采集线程**：`Pipeline::stop` 里先 `stop()` 置标志再 `wait()`，否则可能死等。
- **QImage 用 `scanLine` 访问像素**，别用 `constBits` 做行间线性索引（本项目踩过这个坑，行有对齐）。

---

## 8. 学习资源（按需查）

- 官方文档：`doc.qt.io` 搜 `Signals & Slots`、`QThread`、`QMutex`、`QTcpSocket`
- 本项目三份文档配合读：`README.md`（总览）、`docs/DESIGN.md`（架构）、`docs/USER_MANUAL.md`（使用）

> **学完自检**：能不看代码讲清数据流 + 能独立完成第 4 部分 ≥5 个作业 + 能答出第 5 部分的追问，就算真正入门了。

# VisionInspect —— 工业视觉检测上位机（Qt 6 / C++）

一个面向 **机器视觉上位机** 场景的完整演示项目：相机采集（虚拟/真实）→ 多线程实时检测 → 结果统计与 TCP 上报。
采用**可插拔相机接口** + **生产者-消费者流水线**架构，纯 Qt 自写视觉算法，零第三方依赖。

> 定位：视觉软件工程师（应届生/实习生）求职作品，覆盖岗位要求的 C++/Qt 上位机、多线程实时性、
> 相机抽象、机器视觉、网络通信、单元测试与技术文档等全部要点。

---

## ✨ 功能特性（对应岗位职责）

| 功能 | 说明 | 对应岗位要求 |
|---|---|---|
| 可插拔相机 | `CameraInterface` 抽象；**真实摄像头(Media Foundation)** + 虚拟相机模拟采集；预留工业相机 SDK 接入位 | 相机 SDK 使用、软硬件联调 |
| 多线程流水线 | 采集线程 / 处理线程 / UI 线程通过有界队列解耦，实时刷新 | 多线程、实时性优化 |
| 视觉检测 | 灰度化、阈值分割、8 邻域连通域(Blob)、模板匹配；**工件缺陷检测** + **肤色人脸检测**两种模式 | 机器视觉 |
| 结果统计 | OK/NG 计数、良率实时统计；人脸模式下显示检出人数 | 业务逻辑 |
| TCP 上报 | 自定义帧协议（帧头 + 长度 + JSON + CRC16），断线自动重连 | 网络编程、硬件通信 |
| 参数可调 | 检测模式、工件/缺陷阈值、NG 判定参数运行时调节 | 工程化 |
| 单元测试 | Qt Test：算法、检测器(工件/人脸)、帧队列、通信协议共 11 项用例 | 单元/集成测试 |
| 技术文档 | README + 设计说明 + 用户手册 | 文档编写 |

---

## 🗂 目录结构

```
VisionInspect/
├── CMakeLists.txt            # 顶层构建脚本
├── README.md
├── docs/
│   ├── DESIGN.md             # 软件设计说明
│   └── USER_MANUAL.md        # 用户手册
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp              # 入口
│   ├── camera/               # 相机抽象层（CameraInterface / VirtualCamera / MediaFoundationCamera）
│   ├── core/                 # 多线程流水线（FrameQueue / 采集/处理线程 / Pipeline）
│   ├── vision/               # 视觉算法与检测器（ImageAlgorithms / Detector）
│   ├── comm/                 # TCP 结果上报（TcpReporter）
│   └── ui/                   # 上位机界面（MainWindow）
└── tests/
    ├── CMakeLists.txt
    ├── test_vision.cpp       # Qt Test 单元/集成测试
    └── camera_smoke.cpp      # 摄像头冒烟测试（验证真实采集）
```

---

## 🛠 环境要求

- Qt **6.x**（本项目在 6.11.1 / MinGW 13.1.0 上验证）
- CMake ≥ 3.21
- 编译器：MinGW（与 Qt 匹配）或 MSVC
- Ninja 或 Make

## 🚀 构建与运行

```bash
# 1. 配置（以 Qt 6.11.1 + MinGW 为例）
cmake -S . -B build -G Ninja \
      -DCMAKE_PREFIX_PATH="D:/Qt/6.11.1/mingw_64" \
      -DCMAKE_CXX_COMPILER="D:/Qt/Tools/mingw1310_64/bin/g++.exe" \
      -DCMAKE_BUILD_TYPE=Release

# 2. 编译
cmake --build build --target VisionInspect test_vision -j8

# 3. 运行上位机
./build/src/VisionInspect.exe

# 4. 运行单元测试
./build/tests/test_vision.exe
```

> 运行时如提示缺少 DLL，可先用 Qt 自带的 `windeployqt` 部署：
> `D:/Qt/6.11.1/mingw_64/bin/windeployqt.exe build/src/VisionInspect.exe`

## ✅ 测试结果

- `test_vision`：**11 passed / 0 failed**，覆盖灰度、阈值、连通域、检测器（工件 OK/NG / 人脸）、帧队列、TCP 协议。
- `camera_smoke`：真实摄像头采集自检（`FRAME_OK 1280x720 RGB32`）。

---

## 🎯 设计亮点（面试可讲）

1. **依赖倒置**：上位机只依赖 `CameraInterface` 抽象，接入真实相机只需新增一个实现类，符合开闭原则。
2. **真实采集**：`MediaFoundationCamera` 用 Windows 原生 Media Foundation 枚举并采集摄像头（1280x720 RGB32），零第三方依赖。
3. **背压与实时性**：有界帧队列满时丢弃最旧帧，保证处理端永远消费最新数据，符合实时检测场景。
4. **线程解耦**：采集 / 处理 / UI 三线程通过信号槽跨线程排队通信，互不阻塞。
5. **协议可验证**：TCP 帧结构（头 + 长度 + JSON + CRC16）独立成静态方法，便于单元测试与联调。
6. **可复现构建**：CMake + 标准目录组织，一条命令出可执行程序与测试。

## 🔌 相机说明

- **真实摄像头**：`src/camera/MediaFoundationCamera` 基于 Media Foundation 采集本机摄像头，界面下拉框可选。
- **虚拟相机**：无硬件时模拟采集，便于演示工件检测。
- **接入工业相机**：在 `src/camera` 新增 `RealCamera : CameraInterface`，实现 `open/close/grabFrame` 调用
  工业相机 SDK（海康/巴斯勒/Halcon）即可，`MainWindow` 中切换相机实现即可无缝替换。

## 📚 文档

- 架构与模块设计：[docs/DESIGN.md](docs/DESIGN.md)
- 使用说明：[docs/USER_MANUAL.md](docs/USER_MANUAL.md)

# ------------------------------------------------------------------
# 核心逻辑库 vi_core：相机抽象 + 多线程流水线 + 视觉算法 + 通信
# 不依赖 Widgets，可被 GUI 与单元测试共同链接
# ------------------------------------------------------------------
set(CORE_SOURCES
    camera/CameraInterface.h
    camera/VirtualCamera.h
    camera/VirtualCamera.cpp
    camera/MediaFoundationCamera.h
    camera/MediaFoundationCamera.cpp
    core/FrameQueue.h
    core/FrameQueue.cpp
    core/AcquisitionThread.h
    core/AcquisitionThread.cpp
    core/ProcessingThread.h
    core/ProcessingThread.cpp
    core/Pipeline.h
    core/Pipeline.cpp
    vision/ImageAlgorithms.h
    vision/ImageAlgorithms.cpp
    vision/DetectionResult.h
    vision/Detector.h
    vision/Detector.cpp
    comm/TcpReporter.h
    comm/TcpReporter.cpp
)

add_library(vi_core STATIC ${CORE_SOURCES})
target_include_directories(vi_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vi_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network)
# 真实摄像头：链接 Windows Media Foundation（MinGW 自带导入库）
target_link_libraries(vi_core PUBLIC mfplat mfreadwrite mfuuid mf)

# ------------------------------------------------------------------
# 上位机可执行程序：GUI 层
# ------------------------------------------------------------------
set(APP_SOURCES
    main.cpp
    ui/MainWindow.h
    ui/MainWindow.cpp
)

add_executable(VisionInspect ${APP_SOURCES})
target_link_libraries(VisionInspect PRIVATE vi_core Qt6::Widgets)

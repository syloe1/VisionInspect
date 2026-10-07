cmake_minimum_required(VERSION 3.21)
project(VisionInspect VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 依赖：Widgets(上位机界面) + Network(TCP上报)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(src)

# 单元测试（仅在可测试时启用）
enable_testing()
add_subdirectory(tests)

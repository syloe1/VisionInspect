find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_vision test_vision.cpp)
target_link_libraries(test_vision PRIVATE vi_core Qt6::Test)

add_test(NAME test_vision COMMAND test_vision)

# 摄像头冒烟测试（依赖本机摄像头，不作为常规单测注册）
add_executable(camera_smoke camera_smoke.cpp)
target_link_libraries(camera_smoke PRIVATE vi_core Qt6::Core)

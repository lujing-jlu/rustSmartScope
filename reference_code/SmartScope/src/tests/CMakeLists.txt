# 测试程序

# 添加测试相机同步程序
add_executable(test_camera_sync test_camera_sync.cpp)
target_link_libraries(test_camera_sync PRIVATE 
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    core
    infrastructure
)

# 添加多相机管理器测试程序
add_executable(test_multi_camera test_multi_camera.cpp)
target_link_libraries(test_multi_camera PRIVATE 
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    core
    infrastructure
) 
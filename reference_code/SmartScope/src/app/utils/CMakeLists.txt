set(UTILS_SOURCES
    screenshot_manager.cpp
    hid_com/hid_communication.cpp
    led_controller.cpp
    keyboard_listener.cpp
    device_controller.cpp
)

set(UTILS_HEADERS
    screenshot_manager.h
    hid_com/hid_communication.h
    ${CMAKE_SOURCE_DIR}/include/app/utils/led_controller.h
    ${CMAKE_SOURCE_DIR}/include/app/utils/keyboard_listener.h
    ${CMAKE_SOURCE_DIR}/include/app/utils/device_controller.h
)

add_library(utils STATIC ${UTILS_SOURCES} ${UTILS_HEADERS})

# 寻找HIDAPI库
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(HIDAPI QUIET hidapi-hidraw)
    if(HIDAPI_FOUND)
        target_include_directories(utils PRIVATE ${HIDAPI_INCLUDE_DIRS})
        target_link_libraries(utils PRIVATE ${HIDAPI_LIBRARIES})
    else()
        # 如果pkg-config找不到，尝试直接链接
        message(STATUS "未通过pkg-config找到HIDAPI库，尝试直接链接")
        target_link_libraries(utils PRIVATE hidapi-hidraw)
    endif()
else()
    # 直接尝试链接库
    target_link_libraries(utils PRIVATE hidapi-hidraw)
endif()

target_link_libraries(utils
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    infrastructure
)

target_include_directories(utils
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
) 
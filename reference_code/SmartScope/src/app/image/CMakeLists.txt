set(IMAGE_SOURCES
    image_processor.cpp
)

set(IMAGE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/app/image/image_processor.h
)

# 添加静态库
add_library(image STATIC ${IMAGE_SOURCES} ${IMAGE_HEADERS})

# 链接依赖库
target_link_libraries(image
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    ${OpenCV_LIBS}
)

# 包含头文件路径
target_include_directories(image
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
) 
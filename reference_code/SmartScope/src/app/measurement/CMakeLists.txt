set(MEASUREMENT_SOURCES
    measurement_calculator.cpp
    point_cloud_generator.cpp
)

set(MEASUREMENT_HEADERS
    ${CMAKE_SOURCE_DIR}/include/app/measurement/measurement_calculator.h
    ${CMAKE_SOURCE_DIR}/include/app/measurement/point_cloud_generator.h
)

add_library(measurement STATIC ${MEASUREMENT_SOURCES} ${MEASUREMENT_HEADERS})

target_link_libraries(measurement
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    infrastructure
    ${OpenCV_LIBS}
)

target_include_directories(measurement
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
) 
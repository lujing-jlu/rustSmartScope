cmake_minimum_required(VERSION 3.10)

# Define the app_common library with its sources
add_library(app_common STATIC
    point_cloud_utils.cpp
)

# Add include directories needed by this library
target_include_directories(app_common
    PUBLIC # Use PUBLIC so targets linking app_common get these includes too
        ${CMAKE_CURRENT_SOURCE_DIR} # For point_cloud_utils.hpp
        # Propagate necessary include directories from dependencies
        ${Qt5Widgets_INCLUDE_DIRS}
        ${Qt5Core_INCLUDE_DIRS}
        ${Qt5Gui_INCLUDE_DIRS}
        ${PCL_INCLUDE_DIRS}
        ${EIGEN3_INCLUDE_DIR} # Assuming Eigen3 is found via find_package(Eigen3) in root CMakeLists
    PRIVATE
        # Add private include dirs if any
        ${CMAKE_SOURCE_DIR}/include # Include top-level include dir if needed privately
)

# Link libraries needed by this library
target_link_libraries(app_common
    PUBLIC # Use PUBLIC so targets linking app_common get these links too
        Qt5::Widgets
        Qt5::Core
        Qt5::Gui
        ${PCL_LIBRARIES}
    PRIVATE
        # Add private link libraries if any
) 
cmake_minimum_required(VERSION 3.10)
project(USBCameraCallbackTest)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the Rust library
set(RUST_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/target/release")
set(RUST_LIB_NAME "usb_camera")

# Check if the library exists
if(EXISTS "${RUST_LIB_DIR}/lib${RUST_LIB_NAME}.so")
    set(RUST_LIB_PATH "${RUST_LIB_DIR}/lib${RUST_LIB_NAME}.so")
elseif(EXISTS "${RUST_LIB_DIR}/lib${RUST_LIB_NAME}.a")
    set(RUST_LIB_PATH "${RUST_LIB_DIR}/lib${RUST_LIB_NAME}.a")
else()
    message(FATAL_ERROR "Rust library not found. Please build the Rust library first with: cargo build --release")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create the executables
add_executable(cpp_callback_test examples/cpp_callback_test.cpp)
add_executable(interface_test examples/interface_test.cpp)

# Link the Rust library
target_link_libraries(cpp_callback_test ${RUST_LIB_PATH})
target_link_libraries(interface_test ${RUST_LIB_PATH})

# Link system libraries that Rust needs
target_link_libraries(cpp_callback_test pthread dl)
target_link_libraries(interface_test pthread dl)

# Set runtime path for shared library
set_target_properties(cpp_callback_test PROPERTIES
    INSTALL_RPATH "${RUST_LIB_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
)

set_target_properties(interface_test PROPERTIES
    INSTALL_RPATH "${RUST_LIB_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Print instructions
message(STATUS "To build and run:")
message(STATUS "1. Build Rust library: cargo build --release")
message(STATUS "2. Build C++ test: make")
message(STATUS "3. Run tests:")
message(STATUS "   ./cpp_callback_test")
message(STATUS "   ./interface_test")
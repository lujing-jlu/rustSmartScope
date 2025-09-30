set(INFRASTRUCTURE_SOURCES
    logging/logger.cpp
    logging/log_formatter.cpp
    logging/log_appender.cpp
    config/config_manager.cpp
    exception/app_exception.cpp
)

set(INFRASTRUCTURE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/infrastructure/logging/logger.h
    ${CMAKE_SOURCE_DIR}/include/infrastructure/logging/log_formatter.h
    ${CMAKE_SOURCE_DIR}/include/infrastructure/logging/log_appender.h
    ${CMAKE_SOURCE_DIR}/include/infrastructure/config/config_manager.h
    ${CMAKE_SOURCE_DIR}/include/infrastructure/exception/app_exception.h
)

add_library(infrastructure STATIC ${INFRASTRUCTURE_SOURCES} ${INFRASTRUCTURE_HEADERS})

target_link_libraries(infrastructure
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Threads::Threads
)

target_include_directories(infrastructure
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
) 
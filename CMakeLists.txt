cmake_minimum_required(VERSION 3.21)
project(test-capture VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui WaylandClient Widgets)
find_package(TreelandProtocols REQUIRED)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl gl)

set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/subwindow.h
    src/subwindow.cpp
    src/capture.h
    src/capture.cpp
    src/player.h
    src/player.cpp
)

qt_add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
)

qt_add_resources(${PROJECT_NAME} "assets"
    PREFIX "/"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/images
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/images/watermark.png
)

qt6_generate_wayland_protocol_client_sources(${PROJECT_NAME}
    FILES
        ${TREELAND_PROTOCOLS_DATA_DIR}/treeland-capture-unstable-v1.xml
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::WaylandClient
        Qt6::WaylandClientPrivate
        Qt6::Widgets
        PkgConfig::EGL
)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

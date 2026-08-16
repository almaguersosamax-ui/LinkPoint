cmake_minimum_required(VERSION 3.16)

project(LinkPoint VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

if(WIN32)
    enable_language(RC)
    set(LINKPOINT_WIN_RESOURCES resources/app.rc)
else()
    set(LINKPOINT_WIN_RESOURCES)
endif()

qt_add_executable(LinkPoint WIN32
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/hotspotcontroller.cpp
    src/hotspotcontroller.h
    src/httpserver.cpp
    src/httpserver.h
    resources/resources.qrc
    ${LINKPOINT_WIN_RESOURCES}
)

target_include_directories(LinkPoint PRIVATE src)
target_link_libraries(LinkPoint PRIVATE Qt6::Widgets Qt6::Network)

if(MSVC)
    target_compile_options(LinkPoint PRIVATE /W4)
else()
    target_compile_options(LinkPoint PRIVATE -Wall -Wextra)
endif()

install(TARGETS LinkPoint
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

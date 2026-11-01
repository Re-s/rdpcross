cmake_minimum_required(VERSION 3.16)
project(rdpcross LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ┌──── 构建版本（vrndis 风格：v<base>-<距上次tag提交数>-g<7位hash>） ────
include(${CMAKE_SOURCE_DIR}/cmake/VersionInfo.cmake)
rdpcross_git_version()
# 由 template 展开为 src/version_gen.h
set(APP_VERSION_BASE ${GIT_VERSION_BASE} CACHE STRING "" FORCE)
set(APP_VERSION_HASH ${GIT_VERSION_HASH} CACHE STRING "" FORCE)
set(APP_VERSION_COMMITS ${GIT_VERSION_COMMITS} CACHE STRING "" FORCE)
configure_file(src/version_gen.h.in ${CMAKE_BINARY_DIR}/version_gen.h)
message(STATUS "version: full=${GIT_VERSION_FULL} base=${GIT_VERSION_BASE} commits=${GIT_VERSION_COMMITS} hash=${GIT_VERSION_HASH}")
# └─────────────────────────────────────────────────────────────────────┘

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(X11 REQUIRED)

set(SOURCES
    src/main.cpp
    src/models.cpp
    src/config.cpp
    src/rdpfile.cpp
    src/freerdpargs.cpp
    src/x11helper.cpp
    src/rdpview.cpp
    src/sessionwindow.cpp
    src/settingsdialog.cpp
    src/credentialsdialog.cpp
    src/mainwindow.cpp
)

add_executable(rdpcross ${SOURCES})
target_include_directories(rdpcross PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(rdpcross PRIVATE ${CMAKE_BINARY_DIR})  # 包含 cmake 生成的 version_gen.h
target_link_libraries(rdpcross PRIVATE Qt6::Widgets ${X11_LIBRARIES})
target_compile_options(rdpcross PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

install(TARGETS rdpcross RUNTIME DESTINATION bin)
install(FILES resources/rdpcross.svg
    DESTINATION share/icons/hicolor/scalable/apps)
install(FILES resources/rdpcross.desktop
    DESTINATION share/applications)

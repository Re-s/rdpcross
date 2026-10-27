# Cross-compilation toolchain for aarch64 (arm64)
#
# Usage (on an x86_64 host):
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-arm64.cmake -B build
#
# Prerequisites: aarch64-linux-gnu gcc, libqt6* (arm64), libfreerdp3* (arm64)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# toolchain
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_LINKER aarch64-linux-gnu-ld)
set(CMAKE_AR aarch64-linux-gnu-ar)
set(CMAKE_STRIP aarch64-linux-gnu-strip)

# pkg-config: use the target environment's pkg-config
set(ENV{PKG_CONFIG} aarch64-linux-gnu-pkgconfig)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu /usr/include/aarch64-linux-gnu)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

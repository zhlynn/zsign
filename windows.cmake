
# This file defines the Windows toolchain for use on Linux
# There isn't a strong requirement to support builds on Windows at this time

set(CMAKE_SYSTEM_NAME Linux)

set(TOOLCHAIN_DIR ${PROJECT_BINARY_DIR}/linux/toolchain)


#ExternalProject_Add(mingw_x64
#    PREFIX "${PROJECT_BINARY_DIR}/linux/toolchain"
#    URL https://github.com/mxe/mxe/archive/build-2020-06-06.zip
#)



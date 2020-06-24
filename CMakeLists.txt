cmake_minimum_required(VERSION 3.7)

project(zsign)

include(
    ${CMAKE_ROOT}/Modules/ExternalProject.cmake
)


# The basic procedure is this:
# 1.  Download the MingW cross compilation toolkit for windows
# 2.  Download the MingW cross compilation toolkit for Mac
# 3.  Download the GCC compiler collection for Linux
# 4.



set(
    BUILD_DIR
    ${PROJECT_BINARY_DIR}/build
)

set(
    LINUX_CC_TOOLCHAIN_DIR
    ${BUILD_DIR}/toolchain/linux
)

set(
    MMAN_DIR
    ${BUILD_DIR}/toolchain/linux/mman
)

set(
    OPENSSL_DIRECTORY
    ${BUILD_DIR}/toolchain/linux/openssl
)

# OpenSSL cross-compiled
ExternalProject_Add(
    openssl
    PREFIX ${OPENSSL_DIRECTORY}
    URL https://github.com/openssl/openssl/archive/OpenSSL_1_0_2s.tar.gz
    DOWNLOAD_DIR ${OPENSSL_DIRECTORY}/download
    BINARY_DIR ${OPENSSL_DIRECTORY}/source
    SOURCE_DIR ${OPENSSL_DIRECTORY}/source
    CONFIGURE_COMMAND "<SOURCE_DIR>/config"
    BUILD_COMMAND     "make"
    INSTALL_COMMAND   ""
    TEST_COMMAND      ""
)

#ExternalProject_Add_Step(
#    openssl cfg
#    WORKING_DIRECTORY ${OPENSSL_DIRECTORY}/source
#    ALWAYS 1
#    COMMAND "${OPENSSL_DIRECTORY}/source/config"
#    DEPENDS openssl
#)

#ExternalProject_Add_Step(
#        openssl dobuild
#        WORKING_DIRECTORY ${OPENSSL_DIRECTORY}/source
#        ALWAYS 1
#        COMMAND "$(PWD)"
#        DEPENDS openssl
#)

##  This has to come first
#if(NOT EXISTS ${LINUX_CC_TOOLCHAIN_DIR})
#    file(DOWNLOAD
#            https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Automated%20Builds/mingw-w64-bin_x86_64-linux_20131228.tar.bz2/download
#            ${LINUX_CC_TOOLCHAIN_DIR}/mingw.tar.bz2
#            SHOW_PROGRESS
#            )
#    execute_process(
#            COMMAND
#            ${CMAKE_COMMAND} -E
#            tar xvf ${LINUX_CC_TOOLCHAIN_DIR}/mingw.tar.bz2
#            WORKING_DIRECTORY ${LINUX_CC_TOOLCHAIN_DIR}
#    )
#    set(
#        MINGW_W64_CC
#        ${LINUX_CC_TOOLCHAIN_DIR}/bin/x86_64-w64-mingw32-gcc
#    )
#
#    set(
#        MINGW_W64_CPP
#        ${LINUX_CC_TOOLCHAIN_DIR}/bin/x86_64-w64-mingw32-g++
#    )
#endif()
#
#if(NOT EXISTS ${MMAN_DIR})
#   file(
#       DOWNLOAD
#       https://github.com/sunshower-io/mman-win32/archive/v1.0.tar.gz
#       ${MMAN_DIR}/mman.tar.gz
#       SHOW_PROGRESS
#   )
#   execute_process(
#       COMMAND
#       ${CMAKE_COMMAND} -E
#       tar xvf ${MMAN_DIR}/mman.tar.gz
#       WORKING_DIRECTORY ${MMAN_DIR}
#   )
#endif()
#
#if(NOT EXISTS ${OPENSSL_DIRECTORY})
#    file(
#        DOWNLOAD
#        https://github.com/openssl/openssl/archive/OpenSSL_1_0_2s.tar.gz
#        ${OPENSSL_DIRECTORY}/openssl.tar.gz
#        SHOW_PROGRESS
#    )
#    execute_process(
#        COMMAND
#        ${CMAKE_COMMAND} -E
#        tar xvf ${OPENSSL_DIRECTORY}/openssl.tar.gz
#        WORKING_DIRECTORY ${OPENSSL_DIRECTORY}
#    )
#
#endif()
#






set(CMAKE_CXX_STANDARD 11)

include_directories(.)
include_directories(common)


set(
    OPENSSL_USE_STATIC_LIBS 1
)

set(
    OPENSSL_ROOT_DIR
    ${OPENSSL_DIRECTORY}/source
)
find_package(OpenSSL)

add_executable(
    zsign
    common/base64.cpp
    common/base64.h
    common/common.cpp
    common/common.h
    common/json.cpp
    common/json.h
    common/mach-o.h
    archo.cpp
    archo.h
    bundle.cpp
    bundle.h
    macho.cpp
    macho.h
    openssl.cpp
    openssl.h
    signing.cpp
    signing.h
    zsign.cpp
)

target_link_libraries(zsign OpenSSL::SSL)






cmake_minimum_required(VERSION 3.20)

project(zsign)
set(CMAKE_CXX_STANDARD 11)

# Dependencies

find_package(OpenSSL REQUIRED)
message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
include_directories(${OPENSSL_INCLUDE_DIR})
list(APPEND LIB_LIST ${OPENSSL_LIBRARIES})

find_package(ZLIB REQUIRED)
message("zlib include dir: ${ZLIB_INCLUDE_DIR}")
message("zlib libraries: ${ZLIB_LIBRARIES}")
include_directories(${ZLIB_INCLUDE_DIR})
list(APPEND LIB_LIST ${ZLIB_LIBRARIES})


# Main sources

include_directories(common)

set(COMMON common/base64.cpp common/json.cpp common/common.cpp)
set(SRC ${COMMON} archo.cpp bundle.cpp macho.cpp openssl.cpp signing.cpp zsign.cpp)

add_executable(zsign ${SRC})
target_link_libraries(zsign ${LIB_LIST})


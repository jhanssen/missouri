cmake_minimum_required(VERSION 2.8.4)
include(ExternalProject)

find_program(YASM_PROGRAM "yasm")
if(NOT YASM_PROGRAM)
    message(FATAL_ERROR "yasm is required but could not be found")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(X264_LIBS -L${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(X264_INCS -I${CMAKE_CURRENT_SOURCE_DIR}/include)
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(X264_LIBS "${X264_LIBS} -framework CoreFoundation -framework CoreVideo -framework VideoDecodeAcceleration")
endif()

ExternalProject_Add(
    ffmpeg
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg
    GIT_REPOSITORY "git://source.ffmpeg.org/ffmpeg.git"
    GIT_TAG "n0.11.1"
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/configure --enable-static --prefix="${CMAKE_CURRENT_SOURCE_DIR}" --disable-debug
    PREFIX ${CMAKE_CURRENT_SOURCE_DIR}
    BUILD_COMMAND "$(MAKE)"
    INSTALL_COMMAND "$(MAKE)" install
    )

ExternalProject_Add(
    x264
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/x264
    GIT_REPOSITORY "git://git.videolan.org/x264.git"
    GIT_TAG "37be55213a39db40cf159ada319bd482a1b00680"
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/x264/configure --enable-static --prefix="${CMAKE_CURRENT_SOURCE_DIR}" "--extra-cflags=${X264_INCS}" "--extra-ldflags=${X264_LIBS}"
    PREFIX ${CMAKE_CURRENT_SOURCE_DIR}
    BUILD_COMMAND "$(MAKE)"
    INSTALL_COMMAND "$(MAKE)" install
    )

add_dependencies(x264 ffmpeg)

cmake_minimum_required(VERSION 2.8.6)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_definitions(-DOS_${CMAKE_SYSTEM_NAME})
if(NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

set(client_SRCS
    main.cpp
    Decoder.cpp
    ../common/Host.cpp
    ../common/UdpSocket.cpp
)

set(client_LIBS
    -lpthread
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    list(APPEND client_LIBS -lws2_32)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    find_library(VDADECODER_LIBRARY VideoDecodeAcceleration)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    mark_as_advanced(VDADECODER_LIBRARY
                     COREFOUNDATION_LIBRARY
                     COREVIDEO_LIBRARY)
    set(EXTRA_LIBS ${VDADECODER_LIBRARY})
    find_path(VDADECODER_INCLUDE_DIR VDADecoder.h)
    if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../common ${VDADECODER_INCLUDE_DIR})
add_executable(client ${client_SRCS})
target_link_libraries(client ${client_LIBS} ${VDADECODER_LIBRARY} ${COREFOUNDATION_LIBRARY} ${COREVIDEO_LIBRARY})

cmake_minimum_required(VERSION 2.8.6)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_definitions(-DOS_${CMAKE_SYSTEM_NAME})
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_definitions(-D_WIN32_WINNT=0x0600)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

set(server_LIBS
    ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/lib/libx264.a
    ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/lib/libswscale.a
    ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/lib/libavformat.a
    ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/lib/libavcodec.a
    ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/lib/libavutil.a
    -lpthread -lws2_32
)

set(server_INCS
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../common
    ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/include
    ${CMAKE_CURRENT_LIST_DIR}/../win8dupl
)

set(server_SRCS
    main.cpp
    Win8Capturer.cpp
    Encoder.cpp
    ../common/Receiver.cpp
    ../common/Host.cpp
    ../common/UdpSocket.cpp
    ../common/TcpSocket.cpp
    ../common/TcpServer.cpp
    ../common/Util.cpp
)

include_directories(${server_INCS})
add_executable(server ${server_SRCS})
target_link_libraries(server ${server_LIBS})

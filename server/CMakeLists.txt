cmake_minimum_required(VERSION 2.8.6)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_definitions(-DOS_${CMAKE_SYSTEM_NAME})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(server_LIBS
    ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/lib/libx264.a
    ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/lib/libswscale.a
    ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/lib/libavutil.a
    -lpthread -lws2_32
)

set(server_SRCS
    main.cpp
    GDICapturer.cpp
    Encoder.cpp
    ../common/UdpSocket.cpp
)

include_directories(${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../common ${CMAKE_CURRENT_LIST_DIR}/../3rdparty/include)
add_executable(server ${server_SRCS})
target_link_libraries(server ${server_LIBS})

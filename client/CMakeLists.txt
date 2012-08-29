cmake_minimum_required(VERSION 2.8.6)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_definitions(-DOS_${CMAKE_SYSTEM_NAME})

set(client_SRCS
    main.cpp
    ../common/UdpSocket.cpp
)

set(client_LIBS
    -lpthread
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    list(APPEND client_LIBS -lws2_32)
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_executable(client ${client_SRCS})
target_link_libraries(client ${client_LIBS})

cmake_minimum_required(VERSION 2.8.6)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_definitions(-DOS_${CMAKE_SYSTEM_NAME})

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_executable(client main.cpp)
target_link_libraries(client)

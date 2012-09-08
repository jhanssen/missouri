cmake_minimum_required(VERSION 2.6)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_subdirectory(client)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_subdirectory(server)
endif()

add_custom_target(3rdparty
    COMMAND cmake . && \$\(MAKE\)
    WORKING_DIRECTORY 3rdparty)

add_custom_target(release
    COMMAND cmake -DCMAKE_BUILD_TYPE=Release .
    WORKING_DIRECTORY .)

add_custom_target(debug
    COMMAND cmake -DCMAKE_BUILD_TYPE=Debug .
    WORKING_DIRECTORY .)

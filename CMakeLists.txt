cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/code/app/main"
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(RoomEnvironmentMonitor)

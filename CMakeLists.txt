cmake_minimum_required(VERSION 3.19)

project(vk_sandbox)

set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/utils.cmake)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/src)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
#    add_definitions(/MP)
endif()

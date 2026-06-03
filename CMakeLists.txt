cmake_minimum_required(VERSION 3.10)
project(YearProject CXX)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
	src/main.cpp
	src/disk_manager.cpp
	src/buffer_pool.cpp
	src/bplus_tree.cpp
)

add_executable(ltm_index ${SOURCES})

target_include_directories(ltm_index PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

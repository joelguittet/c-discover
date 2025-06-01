# @file      CMakeLists.txt
# @brief     c-discover library CMakeLists file
#
# MIT License
#
# Copyright joelguittet and c-discover contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# CMake minimum version
cmake_minimum_required(VERSION 3.14)

# Project name
project(c-discover)

# Project version
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION PROJECT_VER LIMIT_COUNT 1)
string(REPLACE "." ";" PROJECT_VER_LIST ${PROJECT_VER})
list(GET PROJECT_VER_LIST 0 PROJECT_VER_MAJOR)
list(GET PROJECT_VER_LIST 1 PROJECT_VER_MINOR)
list(GET PROJECT_VER_LIST 2 PROJECT_VER_PATCH)

# Use GNU installation directories
include(GNUInstallDirs)

# Additional flags
set(c_flags "${c_flags} -Os -ffunction-sections -Wall -fPIC")
set(linker_flags "${linker_flags} -Wl,-gc-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

# Definitions
add_definitions(-DDISCOVER_EXPORT_SYMBOLS -DDISCOVER_API_VISIBILITY)

# CMake subdirectories
if(NOT TARGET cjson)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/cJSON/CMakeLists.txt)
        set(ENABLE_CJSON_TEST OFF CACHE BOOL "Enable building cJSON test")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/cJSON)
    endif()
endif()

# List of sources
file(GLOB_RECURSE src ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c ${CMAKE_CURRENT_SOURCE_DIR}/lib/uuid4/src/*.c)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/uuid4/src)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/cJSON/cJSON.h)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/cJSON)
endif()

# Creation of the library
add_library(discover SHARED ${src})

# Link the library with the wanted libraries
target_link_libraries(discover cjson pthread m)

# Properties of the library
set_target_properties(discover
    PROPERTIES
    SOVERSION "${PROJECT_VER_MAJOR}"
    VERSION "${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}.${PROJECT_VER_PATCH}"
)

# Creation of the examples binaries
option(ENABLE_DISCOVER_EXAMPLES "Enable building discover examples" OFF)
if(ENABLE_DISCOVER_EXAMPLES)
    add_executable(basic ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic.c)
    target_link_libraries(basic discover)
    add_executable(basic-advertise ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic-advertise.c)
    target_link_libraries(basic-advertise discover)
    add_executable(basic-self ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic-self.c)
    target_link_libraries(basic-self discover)
    add_executable(test-multicast ${CMAKE_CURRENT_SOURCE_DIR}/examples/test-multicast.c)
    target_link_libraries(test-multicast discover)
    add_executable(test-unicast ${CMAKE_CURRENT_SOURCE_DIR}/examples/test-unicast.c)
    target_link_libraries(test-unicast discover)
endif()

# Installation
set(CMAKE_INSTALL_FULL_LIBDIR lib)
set(CMAKE_INSTALL_FULL_BINDIR bin)
set(CMAKE_INSTALL_FULL_INCLUDEDIR include)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/discover.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
install(TARGETS discover
    ARCHIVE DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}"
)
if(ENABLE_DISCOVER_EXAMPLES)
    install(TARGETS basic basic-advertise basic-self test-multicast test-unicast
        ARCHIVE DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}"
    )
endif()

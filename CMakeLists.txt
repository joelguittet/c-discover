# CMake minimum version
cmake_minimum_required(VERSION 3.14)

# Project name
project(c-discover)

# Project version
set(DISCOVER_VERSION_MAJOR 1)
set(DISCOVER_VERSION_MINOR 2)
set(DISCOVER_VERSION_PATCH 0)

# Additional flags
set(c_flags "${c_flags} -Os -ffunction-sections -Wall -fPIC")
set(linker_flags "${linker_flags} -Wl,-gc-sections")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

# Definitions
add_definitions(-DDISCOVER_EXPORT_SYMBOLS -DDISCOVER_API_VISIBILITY)

# Output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build/bin)

# CMake subdirectories
if(NOT TARGET cjson)
  if(EXISTS lib/cJSON)
    add_subdirectory(lib/cJSON)
  endif()
endif()

# List of sources
file(GLOB_RECURSE src "src/*.c" "lib/uuid4/src/*.c")

# Add include directories
include_directories(inc)
include_directories(lib/uuid4/src)
if(EXISTS lib/cJSON)
  include_directories(lib/cJSON)
endif()

# Creation of the library
add_library(discover SHARED ${src})

# Link the library with the wanted libraries
target_link_libraries(discover cjson pthread m)

# Properties of the library
set_target_properties(discover
  PROPERTIES
  SOVERSION "${DISCOVER_VERSION_MAJOR}"
  VERSION "${DISCOVER_VERSION_MAJOR}.${DISCOVER_VERSION_MINOR}.${DISCOVER_VERSION_PATCH}"
)

# Creation of the examples binaries
option(ENABLE_DISCOVER_EXAMPLES "Enable building discover examples" OFF)
if(ENABLE_DISCOVER_EXAMPLES)
  add_executable(basic "examples/basic.c")
  target_link_libraries(basic discover)
  add_executable(basic-advertise "examples/basic-advertise.c")
  target_link_libraries(basic-advertise discover)
  add_executable(basic-self "examples/basic-self.c")
  target_link_libraries(basic-self discover)
  add_executable(test-multicast "examples/test-multicast.c")
  target_link_libraries(test-multicast discover)
  add_executable(test-unicast "examples/test-unicast.c")
  target_link_libraries(test-unicast discover)
endif()

# Installation
set(CMAKE_INSTALL_FULL_LIBDIR lib)
set(CMAKE_INSTALL_FULL_BINDIR bin)
set(CMAKE_INSTALL_FULL_INCLUDEDIR include)
if(EXISTS lib/cJSON)
  install(FILES lib/cJSON/cJSON.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
  install(TARGETS cjson
    ARCHIVE DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}"
  )
endif()
install(FILES inc/discover.h DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
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

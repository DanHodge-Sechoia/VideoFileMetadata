cmake_minimum_required(VERSION 3.21)
project(VideoFileMetadata C)

set(CMAKE_C_STANDARD 99)

# Use pkg-config to get Gstreamer
find_package(PkgConfig)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_MPEGTS REQUIRED gstreamer-mpegts-1.0)

# Include external directories
include_directories(
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_MPEGTS_INCLUDE_DIRS})

# Link external libraries
link_directories(
    ${GLIB_LIBRARY_DIRS}
    ${GSTREAMER_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} mpegts.c)

# Link Gstreamer library with target executable
target_link_libraries(${PROJECT_NAME}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_MPEGTS_LIBRARIES})

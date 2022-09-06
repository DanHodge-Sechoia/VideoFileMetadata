cmake_minimum_required(VERSION 2.8)

project(h264bitstream)

SET(GCC_COVERAGE_COMPILE_FLAGS "-std=c99 -pedantic -Wall -W -Wshadow -Wwrite-strings -Wno-unused -g")
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

include_directories(.)

file(GLOB SOURCES "*.c")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/h264_sei.in.c")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/h264_slice_data.in.c")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/h264_stream.in.c")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/svc_split.c")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/h264_avcc.c")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/h264_slice_data.c")

add_library(h264bitstream SHARED ${SOURCES})

#g++ openRTSP.cpp playCommon.cpp -I . -I ../liveMedia/include -I ../liveMedia -I ../groupsock/include -I ../UsageEnvironment/include -I ../BasicUsageEnvironment/include ../liblive555.so -o openRTSP
#LD_LIBRARY_PATH=../ ./openRTSP

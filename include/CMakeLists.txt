# CMakeList.txt : CMake project for ecs, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.15)

add_library(tls INTERFACE
	"replicator.h"
	"splitter.h"
	"cache.h"
target_include_directories(tls INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

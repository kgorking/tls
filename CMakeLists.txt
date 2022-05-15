cmake_minimum_required (VERSION 3.15)

project (tls CXX)

# creates a library 'tls' which is an interface (header files only)
add_library(tls INTERFACE)

install(
    DIRECTORY include/tls
    DESTINATION include
)

# Require c++ 20
set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED on)

# determine whether this is a standalone project or included by other projects
set(TLS_STANDALONE_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(TLS_STANDALONE_PROJECT ON)
endif ()

if (MSVC)
    # warning level 4
    target_compile_options(tls INTERFACE /W4)
else()
    # lots of warnings
    target_compile_options(tls INTERFACE -Wall -Wextra -pedantic)
endif()

# Project headers
# add include folders to the library and targets that consume it
# the SYSTEM keyword suppresses warnings for users of the library
if(TLS_STANDALONE_PROJECT)
    target_include_directories(tls INTERFACE
        $<BUILD_INTERFACE:
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        >
    )
else()
    target_include_directories(tls SYSTEM INTERFACE
        $<BUILD_INTERFACE:
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        >
    )
endif()

if (TLS_STANDALONE_PROJECT)
    # Benchmark
    #set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Suppressing benchmark's tests" FORCE)
    #add_subdirectory ("benchmark/gbench")
    #add_subdirectory ("benchmark")

    # Examples
    add_subdirectory ("examples/collect/accumulate")
    add_subdirectory ("examples/collect/concurrent_fill_vector")
    add_subdirectory ("examples/collect/custom_combiner")
    add_subdirectory ("examples/replicate")
    add_subdirectory ("examples/cache")

    # Unittests
    enable_testing()
    add_subdirectory ("unittest")
endif ()

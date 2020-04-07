cmake_minimum_required (VERSION 3.15)

add_executable (cache "cache.cpp")
target_link_libraries(cache tls)

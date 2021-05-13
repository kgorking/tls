cmake_minimum_required (VERSION 3.15)

add_executable (replicate "replicate.cpp")
target_link_libraries(replicate tls)

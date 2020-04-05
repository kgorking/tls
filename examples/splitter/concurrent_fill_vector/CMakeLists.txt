cmake_minimum_required (VERSION 3.15)

add_executable (concurrent_fill_vector "concurrent_fill_vector.cpp")
target_link_libraries(concurrent_fill_vector tls)

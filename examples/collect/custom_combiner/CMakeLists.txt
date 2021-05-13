cmake_minimum_required (VERSION 3.15)

add_executable (custom_combiner "custom_combiner.cpp")
target_link_libraries(custom_combiner tls)

cmake_minimum_required (VERSION 3.15)

add_executable (accumulate "accumulate.cpp")
target_link_libraries(accumulate tls)

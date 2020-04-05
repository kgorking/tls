cmake_minimum_required (VERSION 3.15)

add_executable (replicator "replicator.cpp")
target_link_libraries(replicator tls)

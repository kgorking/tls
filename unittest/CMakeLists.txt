cmake_minimum_required (VERSION 3.15)

add_executable (unittests
	"unittest.cpp"
	"catch.hpp"

	"collect/collect.cpp"
	"cache/unittest.cpp")
target_link_libraries(unittests tls)

add_test(unittests unittests)

#include <cstdlib> // for rand()
#include <iostream>
#include <vector>
#include <algorithm>
#include <execution>

#include <tls/splitter.h>

// Calculate the square root of elements in a vector
int main()
{
	std::vector<double> input(64 * 1024 /** 1024*/);
	std::generate(std::begin(input), std::end(input), rand);

	tls::splitter<std::vector<double>> vec;
	std::for_each(std::execution::par, input.begin(), input.end(), [&vec](double val) {
		vec.local().push_back(sqrt(val));
	});

	// Combine with a lambda
	std::vector<double> combined_vec;
	std::for_each(vec.begin(), vec.end(), [&combined_vec](std::vector<double> const& a) {
		combined_vec.insert(combined_vec.end(), a.begin(), a.end());
	});
	std::cout << "Result was " << combined_vec.size() << ", expected " << input.size() << '\n';
}

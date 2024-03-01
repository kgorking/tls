#include <vector>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <thread>

#include <tls/collect.h>

// Dumps the content of a vector
void dump(std::vector<unsigned> const& v) {
	for (unsigned val : v)
        std::cout << val << ' ';
}

// Prints the threaded vectors with a '-' to indicate threaded splits
template<typename T>
void dump_threaded(tls::collect<T> const& collector) {
	collector.for_each([](auto const& vec) {
		dump(vec);
		std::cout << "- ";
	});
}

int main() {
    unsigned const num_threads = std::thread::hardware_concurrency();
    int const chunk_size = 10;
	unsigned const num_inputs = num_threads * chunk_size;

    std::cout << "Concurrently push_back data from one vector to another.\n";
    std::cout << "Using " << num_threads << " threads, '-' in the output shows where the vector was split across threads.\n\n";

    // Vector to concurrently iterate over
	std::vector<unsigned> input(num_inputs);
    std::iota(input.begin(), input.end(), 0);

	// The thread-local vector
	tls::collect<std::vector<unsigned>> vec;

	// Create a worker that writes into a thread-local vector
	auto const worker = [&](unsigned start) {
        auto& local = vec.local();
		for (unsigned i = start; i < start + chunk_size; i++)
            local.push_back(i);
    };

    // Run some concurrent code that would normally create a data race
	{
		std::vector<std::jthread> threads;
		for (unsigned thr = 0; thr < num_threads; thr++) {
			threads.emplace_back(worker, thr * chunk_size);
		}
	}

	// Dump some data for verification
	std::cout << "Initial data:\n";
		dump(input);
    std::cout << '\n' << '\n';

    std::cout << "Concurrent push_back result:\n";
	    dump_threaded(vec);
	std::cout << '\n' << '\n';

    std::cout << "Flattened:\n";
		std::vector<unsigned> reduced_vec;
		vec.gather_flattened(std::back_inserter(reduced_vec));
		dump(reduced_vec);
    std::cout << '\n' << '\n';

    std::cout << "Sorted:\n";
		std::sort(reduced_vec.begin(), reduced_vec.end());
		dump(reduced_vec);
    std::cout << '\n' << '\n';

	std::cout << "Matches initial data? " << std::boolalpha << (input == reduced_vec) << '\n';
}

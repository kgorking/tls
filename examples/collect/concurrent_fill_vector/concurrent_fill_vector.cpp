#include <vector>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <thread>

#include <tls/collect.h>

// Dumps the content of a vector
void dump(std::vector<int> const& v) {
    for (int val : v)
        std::cout << val << ' ';
}

// Prints the threaded vectors with a '-' to indicate threaded splits
void dump_threaded(std::vector<std::vector<int>> const& threaded_vec) {
    for (auto const& vec : threaded_vec) {
        dump(vec);
        std::cout << "- ";
    }
    std::cout << '\n' << '\n';
}

// Merges all the threaded vectors in to a single vector
std::vector<int> reduce(std::vector<std::vector<int>> const& threaded_vec) {
    std::vector<int> vec;
    for (auto const& t_vec : threaded_vec) {
        vec.insert(vec.end(), t_vec.begin(), t_vec.end());
    }
    return vec;
}

int main() {
    int const num_threads = std::thread::hardware_concurrency();
    int const chunk_size = 24;
    int const num_inputs = num_threads * chunk_size;

    std::cout << "Concurently push_back data from one vector to another.\n";
    std::cout << "Using " << num_threads << " threads, '-' in the output shows where the vector was split across threads.\n\n";

    // Vector to concurrently iterate over
    std::vector<int> input(num_inputs);
    std::iota(input.begin(), input.end(), 0);
    std::cout << "Initial data:\n";
    dump(input);
    std::cout << '\n' << '\n';

    // Run some concurrent code that would normally create a data race
    tls::collect<std::vector<int>> vec;
    auto const worker = [&vec, chunk_size](int start) {
        auto& local = vec.local();
        for (int i = start; i < start + chunk_size; i++)
            local.push_back(i);
    };

    std::vector<std::thread> threads;
    for (int thr = 0; thr < num_threads; thr++) {
        threads.emplace_back(worker, thr * chunk_size);
    }

    for (auto &thread : threads) {
		thread.join();
	}

    std::cout << "Concurrent push_back result:\n";
	auto const collect = vec.gather();
    dump_threaded(collect);

    std::cout << "Reduced:\n";
    std::vector<int> reduced_vec = reduce(collect);
    dump(reduced_vec);
    std::cout << '\n' << '\n';

    std::cout << "Sorted:\n";
	std::sort(reduced_vec.begin(), reduced_vec.end());
    dump(reduced_vec);
    std::cout << '\n' << '\n';
}

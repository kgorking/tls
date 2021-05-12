#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <execution>
#include <chrono>

#include <tls/splitter.h>

size_t constexpr vec_size = 1024 * 1024;

void accumulate_test() {
    // Dummy vector to concurrently iterate over
    std::vector<double> vec(vec_size, 12);

    // Serialized accumulate
    std::cout << "Serial accumulating content of vector\n";
    auto start = std::chrono::system_clock::now();
    {
        double accumulator = 0;

        std::for_each(vec.begin(), vec.end(), [&accumulator](double i) {
            accumulator += cbrt(i);
        });

        auto time1 = std::chrono::system_clock::now() - start;
        std::cout << " result avg:    " << accumulator / vec.size() << "\n";
        std::cout << " completed in " << time1.count() / 1'000'000.0 << "ms\n";
    }

    // Parallel accumulate
    std::cout << "Concurrently accumulating content of vector\n";
    start = std::chrono::system_clock::now();
    {
        tls::splitter<double> accumulator;

        std::for_each(std::execution::par, vec.begin(), vec.end(), [&accumulator](double i) {
            accumulator.local() += cbrt(i);
        });

        double result = 0;
		accumulator.for_each([&result](double val) { result += val; });

        auto time2 = std::chrono::system_clock::now() - start;
        std::cout << " result avg:    " << result / vec.size() << "\n";
        std::cout << " completed in " << time2.count() / 1'000'000.0 << "ms\n";
    }
}

int main() {
    accumulate_test();
}
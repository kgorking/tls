#include <tls/cache.h>
#include <algorithm>
#include <execution>
#include <vector>
#include <numeric>
#include <cmath>
#include <iostream>

int main() {
    using Key = int;
    using Value = int;
    static Key const empty = -1;

    using cache = tls::cache<Key, Value, empty>;
    std::cout << "cache size is " << sizeof(cache) << " bytes, can hold " << cache::max_entries() << " entries\n";

    // Generate values to fill in the cache
    std::vector<Value> values(1'000'000);
    std::generate(values.begin(), values.end(), []() {
        return rand() % static_cast<Value>(cache::max_entries() + 1);
    });

    auto const calc_val = [](Value val) {
        return static_cast<Value>(std::cbrt(std::tgamma(val)));
    };

    std::atomic<int> num_misses = 0;
    std::atomic<int> num_bad_lookups = 0;

    std::for_each(std::execution::par, values.begin(), values.end(), [&num_misses, &num_bad_lookups, calc_val](Value val) {
        thread_local cache cache;

        auto const cached_val = cache.get_or(val, [&num_misses, calc_val](Value v) {
            num_misses++;
            return calc_val(v);
        });

        // verify the lookup
        if (cached_val != calc_val(val))
            num_bad_lookups++;
    });

    std::cout << values.size() - num_misses << " cache hits\n";
    std::cout << num_misses << " cache misses\n";
    std::cout << num_bad_lookups << " bad lookups\n";
}

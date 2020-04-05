#include <execution>
#include <tls/splitter.h>
#include "../catch.hpp"

TEST_CASE("tls::splitter<> specification") {
    SECTION("new instances are default initialized") {
        struct test {
            int x = 4;
        };
        REQUIRE(tls::splitter<int>().local() == int{});
        REQUIRE(tls::splitter<double>().local() == double{});
        REQUIRE(tls::splitter<test>().local().x == test{}.x);
    }

    SECTION("does not cause data races") {
        std::vector<int> vec(1024 * 1024, 1);
        tls::splitter<int> acc;

        std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const& i) { acc.local() += i; });

        int result = std::reduce(acc.begin(), acc.end());
        REQUIRE(result == 1024 * 1024);
    }

    SECTION("clearing after use cleans up properly") {
        std::vector<int> vec(1024, 1);
        tls::splitter<int> acc;

        std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const& i) { acc.local() += i; });
        acc.clear();

        int result = std::reduce(acc.begin(), acc.end());
        REQUIRE(result == 0);
    }

    SECTION("two instances do not interfere with eachother") {
        tls::splitter<int> acc1, acc2;
        acc1.local() = 42;
        REQUIRE(acc2.local() != 42);
    }

    SECTION("tls::splitter<> variables can be copied") {
        std::vector<int> vec(1024 * 1024, 1);
        tls::splitter<int> acc;

        std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const& i) { acc.local() += i; });

        tls::splitter<int> const acc_copy = acc;

        int const result = std::reduce(acc.begin(), acc.end());
        acc.clear();
        REQUIRE(result == 1024 * 1024);

        int const result_copy = std::reduce(acc_copy.cbegin(), acc_copy.cend());
        REQUIRE(result == result_copy);
    }
}

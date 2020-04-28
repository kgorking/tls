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

    SECTION("multiple instances points to the same data") {
        tls::splitter<int> s1, s2, s3;
        s1.local() = 1;
        s2.local() = 2;
        s3.local() = 3;
        CHECK(s1.local() == 3);
        CHECK(s2.local() == 3);
        CHECK(s3.local() == 3);

        tls::splitter<int, bool> s4;
        tls::splitter<int, char> s5;
        tls::splitter<int, short> s6;
        tls::splitter<int, void> s7;  // same as splitter<int> s1,s2,s3
        s4.local() = 1;
        s5.local() = 2;
        s6.local() = 3;
        CHECK(s4.local() == 1);
        CHECK(s5.local() == 2);
        CHECK(s6.local() == 3);
        CHECK(s7.local() == 3);
    }

    SECTION("tls::splitter<> variables can be copied") {
        std::vector<int> vec(1024 * 1024, 1);
        tls::splitter<int> acc;

        std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const& i) { acc.local() += i; });

        tls::splitter<int> const acc_copy = acc;

        int const result = std::reduce(acc.begin(), acc.end());
        acc.clear();
        CHECK(result == 1024 * 1024);

        int const result_copy = std::reduce(acc_copy.begin(), acc_copy.end());
        CHECK(result == result_copy);
    }
}

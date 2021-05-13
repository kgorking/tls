#include <execution>
#include <tls/collect.h>
#include "../catch.hpp"

TEST_CASE("tls::collect<> specification") {
    SECTION("new instances are default initialized") {
        struct test {
            int x = 4;
        };
        REQUIRE(tls::collect<int>().local() == int{});
        REQUIRE(tls::collect<double>().local() == double{});
        REQUIRE(tls::collect<test>().local().x == test{}.x);
    }

    SECTION("does not cause data races") {
        std::vector<int> vec(1024 * 1024, 1);
        tls::collect<int> acc;

        std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const& i) { acc.local() += i; });

        auto const collect = acc.gather();
        int result = std::reduce(collect.begin(), collect.end());
        REQUIRE(result == 1024 * 1024);
    }

    SECTION("clearing after use cleans up properly") {
        std::vector<int> vec(1024, 1);
        tls::collect<int> acc;

        std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const& i) { acc.local() += i; });
        acc.clear();

        auto const collect = acc.gather();
        int result = std::reduce(collect.begin(), collect.end());
        REQUIRE(result == 0);
    }

    SECTION("multiple instances in same scope points to the same data") {
        tls::collect<int> s1, s2, s3;
        s1.local() = 1;
        s2.local() = 2;
        s3.local() = 3;
        CHECK(s1.local() == 3);
        CHECK(s2.local() == 3);
        CHECK(s3.local() == 3);

        tls::collect<int, bool> s4;
        tls::collect<int, char> s5;
        tls::collect<int, short> s6;
        s4.local() = 1;
        s5.local() = 2;
        s6.local() = 3;
        CHECK(s4.local() == 1);
        CHECK(s5.local() == 2);
        CHECK(s6.local() == 3);
    }

    SECTION("data does not persist between out-of-scope instances") {
		{
			tls::collect<int> s1;
			s1.local() = 1;
			CHECK(s1.local() == 1);
		}

        {
			tls::collect<int> s2;
			CHECK(s2.local() != 1);
		}
    }

}

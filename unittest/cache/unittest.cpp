#include "../catch.hpp"
#include <execution>
#include <tls/cache.h>

TEST_CASE("tls::cache specification") {
	SECTION("takes up a cacheline in size") {
		constexpr tls::cache<int, int, 0> cache1;
		REQUIRE(sizeof(cache1) <= 64UL);

		constexpr tls::cache<short, int, 0> cache2;
		REQUIRE(sizeof(cache2) <= 64UL);

		struct local {
			char s[3];
		};
		constexpr tls::cache<short, local, 0> cache3;
		REQUIRE(sizeof(cache3) <= 64UL);

		struct local2 {
			char s[16];
		};
		constexpr tls::cache<short, local2, 0, 128UL> cache4; // 64b cache-line will fail
		REQUIRE(sizeof(cache4) <= 128UL);
	}

	SECTION("caching works") {
		tls::cache<int, int, -1> cache;

		int calc_count = 0;
		auto const calc_val = [&calc_count](int key) {
			calc_count += 1;
			return key + 16;
		};

		for (int i=0; i<8; i++) {
			cache.get_or(i, calc_val);
		}
		REQUIRE(calc_count == 8);

		// values should be cached, so calc_count should not increase
		for (int i=0; i<8; i++) {
			cache.get_or(i, calc_val);
		}
		REQUIRE(calc_count == 8);
	}
}

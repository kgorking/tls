#include "../catch.hpp"
#include <execution>
#include <thread>
#include <tls/collect.h>

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
		tls::unique_collect<int> acc1;
		tls::unique_collect<int> acc2;

		std::for_each(std::execution::par, vec.begin(), vec.end(), [&](int const i) {
			acc1.local() += 1 * i;
			acc2.local() += 2 * i;
		});

		auto collect = acc1.gather();
		int result1 = std::reduce(collect.begin(), collect.end());
		REQUIRE(result1 == 1024 * 1024);

		collect = acc2.gather();
		int result2 = std::reduce(collect.begin(), collect.end());
		REQUIRE(result2 == 2 * 1024 * 1024);
	}

	SECTION("gather cleans up properly") {
		std::vector<int> vec(1024, 1);
		tls::collect<int> acc;

		std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const i) {
			acc.local() += i;
		});

		acc.for_each([](int const& i) {
			REQUIRE(i > 0);
		});
		(void)acc.gather();
		acc.for_each([](int const& i) {
			REQUIRE(i == 0);
		});
	}

	SECTION("clearing after use cleans up properly") {
		std::vector<int> vec(1024, 1);
		tls::collect<int> acc;

		std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const i) {
			acc.local() += i;
		});

		acc.clear();
		auto const collect = acc.gather();
		for (int val : collect)
			CHECK(val == 0);
	}

	SECTION("multiple instances in same scope points to the same data") {
		tls::collect<int> s1, s2, s3;
		s1.local() = 1;
		s2.local() = 2;
		s3.local() = 3;
		CHECK(s1.local() == 3);
		CHECK(s2.local() == 3);
		CHECK(s3.local() == 3);
	}

	SECTION("data persist between out-of-scope instances") {
		{
			tls::collect<int> s1;
			s1.local() = 1;
		}

		{
			tls::collect<int> s2;
			CHECK(s2.local() == 1);
		}
	}

	SECTION("gather flatten works") {
		std::vector<int> vec(std::thread::hardware_concurrency(), 1);
		tls::collect<std::vector<int>> collector;
		std::atomic_size_t counter = 0;

		std::for_each(std::execution::par, vec.begin(), vec.end(), [&](int const&) {
			collector.local().push_back(2);
			counter += 1;
		});

		vec.clear();
		collector.gather_flattened(std::back_inserter(vec));
		REQUIRE(vec.size() == counter);
		for (int i : vec)
			REQUIRE(2 == i);
	}

	SECTION("data persists after thread deaths") {
		tls::unique_collect<int> collector;

		{
			std::vector<std::jthread> threads;
			for (int i = 0; i < 10; i++) {
				threads.emplace_back([&collector, i]() {
					collector.local() = i;
				});
			}
		}

		auto collection = collector.gather();
		std::sort(collection.begin(), collection.end());
		REQUIRE(collection.size() == 10);
		for (int i = 0; i < 10; i++) {
			CHECK(collection[i] == i);
		}
	}

	SECTION("tls::unique_collect<> are unique") {
		tls::unique_collect<int> s4;
		tls::unique_collect<int> s5;
		tls::unique_collect<int> s6;
		s4.local() = 1;
		s5.local() = 2;
		s6.local() = 3;
		CHECK(s4.local() == 1);
		CHECK(s5.local() == 2);
		CHECK(s6.local() == 3);
	}

	SECTION("xx") {
		tls::collect<float> cf;
		cf.local() += 1.23f;

		cf.for_each([](float const&) {});
		cf.for_each([](float&) {});
	}

	SECTION("can use different containers") {
		tls::collect<int, std::list<int>> cf;
		cf.local() = 132;
		auto collected = cf.gather();
		REQUIRE(collected.front() == 132);
	}
}
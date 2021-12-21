#include "../catch.hpp"
#include <execution>
#include <tls/collect.h>

constexpr bool test_constexpr() {
	tls::collect<int> ce_test;
	return true;
}

constexpr bool test_for_each() {
	tls::collect<int> ce_test;
	ce_test.local() = 4;

	// test for_each
	bool check_for_each = true;
	ce_test.for_each([&check_for_each](int i) { check_for_each = check_for_each && (i == 4); });
	return check_for_each;
}

constexpr bool test_gather() {
	std::vector<int> vec(10, 1);
	tls::collect<int> acc;

	// for_each with an execution policy is not constexpr, for obv. reasons
	std::for_each(/*std::execution::par,*/ vec.begin(), vec.end(), [&acc](int const i) { acc.local() += i; });

	bool is_val_ten = true;
	acc.for_each([&is_val_ten](int const& i) { is_val_ten = is_val_ten && (i == 10); });

	auto const gathered = acc.gather();
	return gathered.size() == 1 && gathered.front() == 10 && acc.local() == int{};
}

constexpr bool test_reset() {
	std::vector<int> vec(1024, 1);
	tls::collect<int> acc;

	std::for_each(vec.begin(), vec.end(), [&acc](int const i) { acc.local() += i; });
	acc.reset();

	auto const collect = acc.gather();
	return 0 == collect.size();
}

constexpr bool test_gather_flatten() {
	std::vector<int> vec(17 /*std::thread::hardware_concurrency()*/, 1);
	tls::collect<std::vector<int>> collector;
	size_t counter = 0;

	std::for_each(/*std::execution::par,*/ vec.begin(), vec.end(), [&](int const) {
		collector.local().push_back(2);
		counter += 1;
	});

	vec.clear();
	collector.gather_flattened(std::back_inserter(vec));
	if (vec.size() != counter)
		return false;

	for (int i : vec)
		if (2 != i)
			return false;
	return true;
}


TEST_CASE("tls::collect<> specification") {
	SECTION("constexpr friendly") {
		static_assert(test_constexpr());
		static_assert(test_for_each());
		static_assert(test_gather());
		static_assert(test_reset());
		static_assert(test_gather_flatten());
	}

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

		std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const i) { acc.local() += i; });

		auto const collect = acc.gather();
		int result = std::reduce(collect.begin(), collect.end());
		REQUIRE(result == 1024 * 1024);
	}

	SECTION("gather cleans up properly") {
		std::vector<int> vec(1024, 1);
		tls::collect<int> acc;

		std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const i) { acc.local() += i; });

		acc.for_each([](int const& i) { REQUIRE(i > 0); });
		(void)acc.gather();
		acc.for_each([](int const& i) { REQUIRE(i == 0); });
	}

	SECTION("reseting after use cleans up properly") {
		std::vector<int> vec(1024, 1);
		tls::collect<int> acc;

		std::for_each(std::execution::par, vec.begin(), vec.end(), [&acc](int const i) { acc.local() += i; });
		acc.reset();

		auto const collect = acc.gather();
		REQUIRE(collect.size() == 0);
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

	SECTION("gather flatten works") {
		std::vector<int> vec(std::thread::hardware_concurrency(), 1);
		tls::collect<std::vector<int>> collector;
		std::atomic_int counter = 0;

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
}

#ifndef TLS_COLLECT_H
#define TLS_COLLECT_H

#include <mutex>
#include <shared_mutex>
#include <vector>
#include <concepts>
#include <forward_list>

namespace tls {
// Provides a thread-local instance of the type T for each thread that
// accesses it. Data is preserved when threads die.
// You can collect the thread_local T's with collect::gather*,
// which also resets the data on the threads by moving it.
// Use `tls::unique_collect` or pass different types to 'UnusedDifferentiatorType'
// to create different types.
template <typename T, typename Container = std::vector<T>, typename UnusedDifferentiatorType = void>
class collect final {
	// This struct manages the instances that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the collect<> instance that spawned it.
	struct thread_data final {
		thread_data() {
			collect::init_thread(this);
		}

		~thread_data() {
			collect::remove_thread(this);
		}

		// Return a reference to an instances local data
		[[nodiscard]] T& get() noexcept {
			return data;
		}

		void remove() noexcept {
			data = {};
			next = nullptr;
		}

		[[nodiscard]] T* get_data() noexcept {
			return &data;
		}
		[[nodiscard]] T const* get_data() const noexcept {
			return &data;
		}

		void set_next(thread_data* ia) noexcept {
			next = ia;
		}
		[[nodiscard]] thread_data* get_next() noexcept {
			return next;
		}
		[[nodiscard]] thread_data const* get_next() const noexcept {
			return next;
		}

	private:
		T data{};
		thread_data* next = nullptr;
	};

private:
	static constexpr bool has_container = not std::same_as<Container, std::nullptr_t>;

	// the head of the threads
	inline static std::forward_list<thread_data*> head{};

	// Mutex for serializing access for adding/removing thread-local instances
	inline static std::shared_mutex mtx;

	// Adds a new thread
	static void init_thread(thread_data* t) {
		std::unique_lock sl(mtx);
		head.push_front(t);
	}

	// Removes the thread
	static void remove_thread(thread_data* t) {
		std::unique_lock sl(mtx);

		if constexpr (has_container) {
			// Take the thread data
			T* local_data = t->get_data();
			collected_data().push_back(static_cast<T&&>(*local_data));
		}

		// Remove the thread from the linked list
		head.remove(t);
	}

	// Returns all the data collected from threads
	static Container& collected_data()
		requires(has_container)
	{
		static Container data{};
		return data;
	}

public:
	// Get the thread-local variable
	[[nodiscard]] static T& local() noexcept {
		thread_local thread_data var{};
		return var.get();
	}

	// Gathers all the threads data and returns it. This clears all stored data.
	[[nodiscard]]
	static Container gather()
		requires(has_container)
	{
		std::unique_lock sl(mtx);

		auto& data = collected_data();
		for (thread_data* thread : head) {
			data.push_back(std::move(*thread->get_data()));
			*thread->get_data() = T{};
		}

		return std::move(data);
	}

	// Gathers all the threads data and sends it to the output iterator. This clears all stored data.
	static void gather_flattened(auto dest_iterator)
		requires(std::ranges::range<T> && has_container)
	{
		std::unique_lock sl(mtx);

		for (T& per_thread_data : collected_data()) {
			std::move(per_thread_data.begin(), per_thread_data.end(), dest_iterator);
		}
		collected_data().clear();

		for (thread_data* thread : head) {
			T* ptr_per_thread_data = thread->get_data();
			std::move(ptr_per_thread_data->begin(), ptr_per_thread_data->end(), dest_iterator);
			//*ptr_per_thread_data = T{};
			ptr_per_thread_data->clear();
		}
	}

	// Perform an action on all threads data
	template <class Fn>
	static void for_each(Fn&& fn) {
		if constexpr (std::invocable<Fn, T const&>) {
			std::shared_lock sl(mtx);
			for (thread_data const* thread : head) {
				fn(*thread->get_data());
			}

			if constexpr (has_container)
				for (auto const& d : collected_data())
					fn(d);
		} else {
			std::unique_lock sl(mtx);
			for (thread_data* thread : head) {
				fn(*thread->get_data());
			}

			if constexpr (has_container)
				for (auto& d : collected_data())
					fn(d);
		}
	}

	// Clears all data
	static void clear() {
		std::unique_lock sl(mtx);
		for (thread_data* thread : head) {
			*(thread->get_data()) = {};
		}

		if constexpr (has_container)
			collected_data().clear();
	}
};

template <typename T, typename Container = std::vector<T>, auto U = [] {}>
using unique_collect = collect<T, Container, decltype(U)>;

} // namespace tls

#endif // !TLS_COLLECT_H

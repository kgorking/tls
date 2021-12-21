#ifndef TLS_COLLECT_H
#define TLS_COLLECT_H

#include <mutex>
#include <vector>

namespace tls {
// Works like tls::split, except data is preserved when threads die.
// You can collect the thread_local T's with collect::gather,
// which also resets the data on the threads by moving it.
// Note: Two runtime collect<T> instances in the same thread will point to the same data.
//       If they are evaluated in a constant context each instance has their own data.
//       Differentiate between runtime versions by passing different types to 'UnusedDifferentiaterType',
// 	     so fx collect<int, struct A> is different from collect<int, struct B>.
//       As the name implies, 'UnusedDifferentiaterType' is not used internally, so just put whatever.
template <typename T, typename UnusedDifferentiaterType = void>
class collect {
	// This struct manages the instances that access the thread-local data.
	// Its lifetime is marked as thread_local, which means that it can live longer than
	// the splitter<> instance that spawned it.
	struct thread_data {
		constexpr ~thread_data() noexcept {
			if (owner != nullptr) {
				owner->remove_thread(this);
			}
		}

		// Return a reference to an instances local data
		[[nodiscard]] constexpr T& get(collect* instance) noexcept {
			// If the owner is null, (re-)initialize the thread.
			// Data may still be present if the thread_local instance is still active
			if (owner == nullptr) {
				data = {};
				owner = instance;
				instance->init_thread(this);
			}
			return data;
		}

		constexpr void remove(collect* instance) noexcept {
			if (owner == instance) {
				data = {};
				owner = nullptr;
				next = nullptr;
			}
		}

		[[nodiscard]] constexpr T* get_data() noexcept {
			return &data;
		}
		[[nodiscard]] constexpr T const* get_data() const noexcept {
			return &data;
		}

		constexpr void set_next(thread_data* ia) noexcept {
			next = ia;
		}
		[[nodiscard]] constexpr thread_data* get_next() noexcept {
			return next;
		}
		[[nodiscard]] constexpr thread_data const* get_next() const noexcept {
			return next;
		}

	private:
		T data{};
		collect* owner{};
		thread_data* next = nullptr;
	};

private:
	// the head of the threads that access this splitter instance
	thread_data* head{};

	// All the data collected from threads
	std::vector<T> data{};

	// Data that is only used in constexpr evaluations
	thread_data consteval_data;

	// Mutex for serializing access for adding/removing thread-local instances
	[[nodiscard]] std::mutex& get_runtime_mutex() noexcept {
		static std::mutex s_mtx{};
		return s_mtx;
	}

	// Adds a new thread
	constexpr void init_thread(thread_data* t) noexcept {
		auto const init_thread_imp = [&]() noexcept {
			t->set_next(head);
			head = t;
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			init_thread_imp();
		} else {
			init_thread_imp();
		}
	}

	// Removes the thread
	constexpr void remove_thread(thread_data* t) noexcept {
		auto const remove_thread_impl = [&]() noexcept {
			// Take the thread data
			T* local_data = t->get_data();
			data.push_back(std::move(*local_data));

			// Reset the thread data
			*local_data = T{};

			// Remove the thread from the linked list
			if (head == t) {
				head = t->get_next();
			} else {
				auto curr = head;
				while (curr->get_next() != nullptr) {
					if (curr->get_next() == t) {
						curr->set_next(t->get_next());
						return;
					} else {
						curr = curr->get_next();
					}
				}
			}
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			remove_thread_impl();
		} else {
			remove_thread_impl();
		}
	}

public:
	constexpr collect() noexcept {}
	constexpr collect(collect const&) = delete;
	constexpr collect(collect&&) noexcept = default;
	constexpr collect& operator=(collect const&) = delete;
	constexpr collect& operator=(collect&&) noexcept = default;
	constexpr ~collect() noexcept {
		reset();
	}

	// Get the thread-local thread of T
	[[nodiscard]] constexpr T& local() noexcept {
		if (!std::is_constant_evaluated()) {
			auto const local_impl = [&]() -> T& {
				thread_local thread_data var{};
				return var.get(this);
			};
			return local_impl();
		} else {
			return consteval_data.get(this);
		}
	}

	// Gathers all the threads data and returns it. This clears all stored data.
	[[nodiscard]] constexpr std::vector<T> gather() noexcept {
		auto const gather_impl = [&]() noexcept {
			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				data.push_back(std::move(*thread->get_data()));
				*thread->get_data() = T{};
			}

			return std::move(data);
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			return gather_impl();
		} else {
			return gather_impl();
		}
	}

	// Gathers all the threads data and sends it to the output iterator. This clears all stored data.
	constexpr void gather_flattened(auto dest_iterator) noexcept {
		auto const gather_flattened_impl = [&]() noexcept {
			for (T& t : data) {
				std::move(t.begin(), t.end(), dest_iterator);
			}
			data.clear();

			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				T* ptr_t = thread->get_data();
				std::move(ptr_t->begin(), ptr_t->end(), dest_iterator);
				*ptr_t = T{};
			}
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			gather_flattened_impl();
		} else {
			gather_flattened_impl();
		}
	}

	// Perform an action on all threads data
	template <class Fn>
	constexpr void for_each(Fn&& fn) noexcept {
		auto const for_each_impl = [&]() noexcept {
			for (thread_data* thread = head; thread != nullptr; thread = thread->get_next()) {
				fn(*thread->get_data());
			}

			std::for_each(data.begin(), data.end(), std::forward<Fn>(fn));
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			for_each_impl();
		} else {
			for_each_impl();
		}
	}

	// Resets all data and threads
	constexpr void reset() noexcept {
		auto const reset_impl = [&]() noexcept {
			for (thread_data* thread = head; thread != nullptr;) {
				auto next = thread->get_next();
				thread->remove(this);
				thread = next;
			}

			head = nullptr;
			data.clear();
		};

		if (!std::is_constant_evaluated()) {
			std::scoped_lock sl(get_runtime_mutex());
			reset_impl();
		} else {
			reset_impl();
		}
	}
};
} // namespace tls

#endif // !TLS_COLLECT_H

#ifndef __TLS_REPLICATOR_H
#define __TLS_REPLICATOR_H

#include <atomic>
#include <shared_mutex>
#include <type_traits>
#include <vector>

namespace tls {
	template <class T>
	class replicator {
		struct instance_data {
			replicator<T>* owner{ nullptr };
			T data_copy;
			bool invalidated{ };
		};

		// This struct manages the per-thread data
		struct thread_data {
			using iterator = typename std::vector<instance_data>::iterator;

			thread_data(replicator<T>& instance, T const& t) {
				set_data(instance, t);
			}
			~thread_data() {
				// Remove this instance_data from any replicator<> instances that has accessed it
				for (instance_data& inst : instances) {
					if (nullptr != inst.owner) {
						inst.owner->remove_thread(*this);
					}
				}
			}

			// Find the instance iterator. It needs the caller instance for internal bookkeeping
			iterator find(replicator<T>& instance) {
				// Find the thread_local_data and return its local value
				auto const it = std::find_if(instances.begin(), instances.end(),
											 [&instance](auto const& acc) { return &instance == acc.owner; });
				if (it != instances.end()) {
					return it;
				}

				// No instance_data was found, so this is the first time it has been accessed (ie. new thread)
				instance.init_thread(*this);
				instance_data id{ &instance, instance.base_data(), false };
				instances.push_back(id);
				return instances.end() - 1;
			}

			// Marks the data as out-of-date
			void invalidate(replicator<T>& instance) {
				auto const it = find(instance);
				it->invalidated = true;
			}

			// Returns a copy of the local data
			T const& read(replicator<T>& instance) {
				auto const it = find(instance);
				update_data(*it);
				return it->data_copy;
			}

			template<class Fn>
			auto read(replicator<T>& instance, Fn f) {
				auto const it = find(instance);
				update_data(*it);

				// Only pass const-ref to the data to the function
				T const& const_data = it->data_copy;
				return f(const_data);
			}

			// Sets the local data
			void set_data(replicator<T>& instance, T const& t) {
				auto const it = find(instance);
				it->data_copy = t;
			}

			// Resets this instances owner to null
			void reset_owner(replicator<T>& instance) {
				auto const it = find(instance);
				it->owner = nullptr;
			}

			void remove(replicator<T>& instance) {
				//instances.remove_if([instance](auto& acc) { return instance == acc.owner; });
				auto end = std::remove_if(instances.begin(), instances.end(),
										  [instance](auto& acc) { return instance == acc.owner; });
				instances.erase(end, instance.end());
			}

		private:
			// Check to see if the data needs updating
			void update_data(instance_data& id) {
				if (id.invalidated) {
					id.invalidated = false;

					std::shared_lock sl(id.owner->mtx);
					id.data_copy = id.owner->data;
				}
			}

			std::vector<instance_data> instances{};
		};
		friend thread_data;

		// Store a thread_data and set its data.
		void init_thread(thread_data& t) {
			std::unique_lock ul(mtx);

			threads.push_back(&t);
		}

		// Removes a thread. Happens when they expire
		void remove_thread(thread_data& t) {
			std::unique_lock ul(mtx);

			auto const it = std::find(threads.cbegin(), threads.cend(), &t);
			threads.erase(it);
		}

		// Get the thread-local instance
		thread_data& local() {
			thread_local thread_data inst{ *this, data };
			return inst;
		}

	public:
		replicator(T&& t) : data(std::forward<T>(t)) {}
		replicator(T const& t) : data(t) {}

		~replicator() {
			for (auto inst : threads)
				inst->reset_owner(*this);
		}

		T const& read() {
			return local().read(*this);
		}

		template<class Fn>
		auto read(Fn f) {
			return local().read(*this, f);
		}

		void write(T const& t) {
			// Take a unique lock on the data so no other thread can get to it
			std::unique_lock ul(mtx);

			// Write the data
			data = t;

			// Notify all threads that the data has changed
			for (auto thread : threads)
				thread->invalidate(*this);
		}

		T const& base_data() const {
			return data;
		}

	private:
		// the threads that access this instance
		std::vector<thread_data*> threads;

		// The central data that is replicated to threads
		T data;

		// Mutex to serialize access to the data when it is modified
		std::shared_mutex mtx;
	};
}
#endif // !__TLS_REPLICATOR_H

#ifndef __TLS_REPLICATOR_H
#define __TLS_REPLICATOR_H

#include <shared_mutex>
#include <vector>

namespace tls {
    /// <summary>
    /// This class can be used to replicate data from one writer thread to other reader threads.
    /// Data written to the replicator is copied to all threads that read
    /// from it, with minimal use of locking.
    ///
    /// The writer thread locks when data is written. Reader threads only lock when 
    /// trying to read modified data, when it's not modified it reads from its local copy.
    /// </summary>
    /// <typeparam name="T">The type to replicate to other threads</typeparam>
    template <class T>
    class replicator {
        // Holds the threads local copy of an instances data
        struct local_instance_copy {
            replicator<T>* owner{ nullptr };
            T data_copy;
            bool invalidated{ };
        };

        // This struct manages the per-thread data
        struct thread_data {
            using iterator = typename std::vector<local_instance_copy>::iterator;

            // The thread is dead, so remove references to it from all instances
            ~thread_data() {
                for (local_instance_copy& inst : instances) {
                    if (nullptr != inst.owner) {
                        inst.owner->remove_thread(*this);
                    }
                }
            }

            // Find the instance, returns an iterator. It needs the caller instance for internal bookkeeping
            iterator find(replicator<T>& instance) {
                // Find the thread_local_data and return its local value
                auto const it = std::find_if(instances.begin(), instances.end(),
                                             [&instance](auto const& acc) { return &instance == acc.owner; });
                if (it != instances.end()) {
                    return it;
                }

                // No local_instance_copy was found, so this is the first time it has been accessed (ie. new thread)
                instance.init_thread(*this);
                local_instance_copy id{ &instance, instance.base_data(), false };
                instances.push_back(id);
                return instances.end() - 1;
            }

            // Marks the data as out-of-date
            void invalidate(replicator<T>& instance) {
                auto const it = find(instance);
                it->invalidated = true;
            }

            // Returns the local data
            T const& read(replicator<T>& instance) {
                auto const it = find(instance);
                update_data(*it);
                return it->data_copy;
            }

            // Pass the local data to a user function
            template<class Fn>
            auto read(replicator<T>& instance, Fn f) {
                auto const it = find(instance);
                update_data(*it);

                // Only pass const-ref to the data to the function
                T const& const_data = it->data_copy;
                return f(const_data);
            }

            // Sets a copy of the data for an instance
            void set_data(replicator<T>& instance, T const& t) {
                auto const it = find(instance);
                it->data_copy = t;
            }

            // Resets the instances owner to null
            void reset_owner(replicator<T>& instance) {
                auto const it = find(instance);
                it->owner = nullptr;
            }

            // Removes an instance from the thread
            void remove(replicator<T>& instance) {
                auto const end = std::remove_if(instances.begin(), instances.end(),
                                                [instance](auto& acc) { return instance == acc.owner; });
                instances.erase(end, instance.end());
            }

        private:
            // Check to see if the data needs updating
            void update_data(local_instance_copy& id) {
                if (id.invalidated) {
                    id.invalidated = false;

                    std::shared_lock sl(id.owner->mtx);
                    id.data_copy = id.owner->data;
                }
            }

            std::vector<local_instance_copy> instances{};
        };
        friend thread_data;

        // Store a thread_data in this instance
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

        // Get the thread_data instance for the current thread
        thread_data& local() {
            thread_local thread_data inst{};
            return inst;
        }

    public:
        replicator(T&& t) : data(std::forward<T>(t)) {}
        replicator(T const& t) : data(t) {}

        ~replicator() {
            for (auto thr_data : threads)
                thr_data->reset_owner(*this);
        }

        // Read the threads copy of data
        T const& read() {
            return local().read(*this);
        }

        // Pass the threads copy of data to a user function
        template<class Fn>
        auto read(Fn f) {
            return local().read(*this, f);
        }

        // Set the master data
        void write(T const& t) {
            // Take a unique lock on the data so no other thread can modify it
            std::unique_lock ul(mtx);

            // Write the data
            data = t;

            // Notify all threads that the data has changed
            for (auto thread : threads)
                thread->invalidate(*this);
        }

        // Get the master data
        T const& base_data() const {
            return data;
        }

    private:
        // the threads that access this instance. Changes to 'data' will be replicated to these threads
        std::vector<thread_data*> threads;

        // The central data that is replicated to threads
        T data;

        // Mutex to serialize access to 'data' when it is modified
        std::shared_mutex mtx;
    };
}
#endif // !__TLS_REPLICATOR_H

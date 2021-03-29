#ifndef TLS_SPLITTER_H
#define TLS_SPLITTER_H

#include <mutex>
#include <vector>
#include <forward_list>
#include <algorithm>
#ifdef _MSC_VER
#include <new>  // for std::hardware_destructive_interference_size
#endif

namespace tls {

    namespace detail {
        // Cache-line aware allocator for std::forward_list
        // This is needed to ensure that data is forced into seperate cache lines
        template<class T> // T is the list node containing the type
        struct cla_forward_list_allocator {
            using value_type = T;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;
            using is_always_equal = std::true_type;

#ifdef _MSC_VER
            static constexpr auto cache_line_size = std::hardware_destructive_interference_size;
            static constexpr auto alloc_size = sizeof(T); // msvc already keeps nodes on seperate cache lines
#else // gcc/clang (libc++) needs some masaging
            static constexpr auto cache_line_size = 64UL; // std::hardware_destructive_interference_size not implemented in libc++
            static constexpr auto fl_node_size = 16UL; // size of a forward_list node
            static constexpr auto alloc_size = std::max(sizeof(T), cache_line_size - fl_node_size);
#endif

            constexpr cla_forward_list_allocator() noexcept {}
            constexpr cla_forward_list_allocator(const cla_forward_list_allocator&) noexcept = default;
            template <class other>
            constexpr cla_forward_list_allocator(const cla_forward_list_allocator<other>&) noexcept {}

            T* allocate(std::size_t n) {
                return static_cast<T*>(malloc(n * alloc_size));
            }

            void deallocate(T* p, std::size_t /*n*/) {
                free(p);
            }
        };
    }

    // Provides a thread-local instance of the type T for each thread that
    // accesses it. This avoid having to use locks to read/write data.
    // This class only locks when a new thread is created/destroyed.
    // The set of instances can be accessed through the begin()/end() iterators.
    // Note: Two splitter<T> instances in the same thread will point to the same data.
    //       Differentiate between them by passing different types to 'UnusedDifferentiaterType'.
    //       As the name implies, it's not used internally, so just put whatever.
    template <typename T, typename UnusedDifferentiaterType = void>
    class splitter {
        // This struct manages the instances that access the thread-local data.
        // Its lifetime is marked as thread_local, which means that it can live longer than
        // the splitter<> instance that spawned it.
        struct instance_access {
            // Return a reference to an instances local data
            T& get(splitter<T, UnusedDifferentiaterType>* instance) {
                if (owner == nullptr) {
                    // First-time access
                    owner = instance;
                    data = instance->init_thread(this);
                }

                return *data;
            }

            void remove(splitter<T, UnusedDifferentiaterType>* instance) noexcept {
                if (owner == instance) {
                    owner = nullptr;
                    data = nullptr;
                }
            }

        private:
            splitter<T, UnusedDifferentiaterType>* owner{};
            T* data{};
        };
        friend instance_access;

        // the threads that access this instance
        std::vector<instance_access*> instances;

        // instance-data created by each thread.
        // list contents are not invalidated when more items are added, unlike a vector
        std::forward_list<T, detail::cla_forward_list_allocator<T>> data;

protected:
        // Adds a instance_access and allocates its data.
        // Returns a pointer to the data
        T* init_thread(instance_access* t) {
            // Mutex for serializing access for creating/removing locals
            static std::mutex mtx_storage;
            std::scoped_lock sl(mtx_storage);

            instances.push_back(t);
            data.emplace_front(T{});
            return &data.front();
        }

        // Remove the instance_access. The data itself is preserved
        void remove_thread(instance_access* t) noexcept {
            auto it = std::find(instances.begin(), instances.end(), t);
            if (it != instances.end()) {
                std::swap(*it, instances.back());
                instances.pop_back();
            }
        }

    public:
        ~splitter() noexcept {
            clear();
        }

        using iterator = typename decltype(data)::iterator;
        using const_iterator = typename decltype(data)::const_iterator;

        iterator begin() noexcept { return data.begin(); }
        iterator end() noexcept { return data.end(); }
        const_iterator begin() const noexcept { return data.begin(); }
        const_iterator end() const noexcept { return data.end(); }

        // Get the thread-local instance of T for the current instance
        T& local() {
            thread_local instance_access var{};
            return var.get(this);
        }

        // Clears all the thread instances and data
        void clear() noexcept {
            for (instance_access* instance : instances)
                instance->remove(this);
            instances.clear();
            data.clear();
        }

        // Sort the data using std::less<>
        void sort() {
            data.sort();
        }

        // Sort the data using supplied predicate 'bool(auto const&, auto const&)'
        template <class Predicate>
        void sort(Predicate pred) {
            data.sort(pred);
        }
    };
}

#endif // !TLS_SPLITTER_H

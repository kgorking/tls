#ifndef __TLS_CACHE
#define __TLS_CACHE

#include <array>

namespace tls {
    // A class using a cache-line to cache data
    template <class Key, class Value, Key empty_slot = Key{}, size_t cache_line = 64UL>
    class cache {
    public:
        static constexpr size_t max_entries = (cache_line) / (sizeof Key + sizeof Value);

        cache() noexcept {
            keys.fill(empty_slot);
        }

        template <class Fn>
        Value get_or(Key const& k, Fn or_fn) {
            auto const index = find_index(k);
            if (index < max_entries)
                return values[index];

            insert_val(k, or_fn(k));
            return values[0];
        }

    protected:
        void insert_val(Key const& k, Value v) {
            // Move all but last pair one step to the right
            std::move_backward(keys.begin(), keys.end() - 1, keys.end());
            std::move_backward(values.begin(), values.end() - 1, values.end());

            // Insert the new pair at the front of the cache
            keys[0] = k;
            values[0] = std::move(v);
        }

        std::size_t find_index(Key const& k) const {
            auto const it = std::find(keys.begin(), keys.end(), k);
            if (it == keys.end())
                return max_entries;
            return std::distance(keys.begin(), it);
        }

    private:
        std::array<Key, max_entries> keys{};
        std::array<Value, max_entries> values{};
    };

}

#endif // !__TLS_CACHE

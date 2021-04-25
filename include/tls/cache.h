#ifndef TLS_CACHE
#define TLS_CACHE

#include <algorithm>

namespace tls {
    // A class using a cache-line to cache data.
    template <class Key, class Value, Key empty_slot = Key{}, size_t cache_line = 64UL>
    class cache {
        // If you trigger this assert, then either your key- or value size is too large,
        // or you cache_line size is too small.
        // The cache should be able to hold at least 4 key/value pairs in order to be efficient.
	    static_assert((sizeof(Key) + sizeof(Value)) <= (cache_line / 4), "key or value size too large");

        static constexpr size_t num_entries = (cache_line) / (sizeof(Key) + sizeof(Value));

    public:
        constexpr cache() {
            reset();
        }

        // Returns the value if it exists in the cache,
        // otherwise inserts 'or_fn(k)' in cache and returns it
        template <class Fn>
        constexpr Value get_or(Key const k, Fn or_fn) {
            auto const index = find_index(k);
            if (index < num_entries)
                return values[index];

            insert_val(k, or_fn(k));
            return values[0];
        }

        // Clears the cache
        constexpr void reset() {
            std::fill(keys, keys + num_entries, empty_slot);
            std::fill(values, values + num_entries, Value{});
        }

        // Returns the number of key/value pairs that can be cached
        static constexpr size_t max_entries() {
			return num_entries;
        }

    protected:
        constexpr void insert_val(Key const k, Value const v) {
            // Move all but last pair one step to the right
            std::move_backward(keys, keys + num_entries - 1, keys + num_entries);
            std::move_backward(values, values + num_entries - 1, values + num_entries);

            // Insert the new pair at the front of the cache
            keys[0] = k;
            values[0] = v;
        }

        constexpr std::size_t find_index(Key const k) const {
            auto const it = std::find(keys, keys + num_entries, k);
            if (it == keys + num_entries)
                return num_entries;
            return std::distance(keys, it);
        }

    private:
        Key keys[num_entries];
        Value values[num_entries];
    };

}

#endif // !TLS_CACHE

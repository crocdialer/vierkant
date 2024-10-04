//
// Created by Croc Dialer on 28.09.24.
//

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>

#include <crocore/utils.hpp>
#include <vierkant/hash.hpp>

namespace vierkant
{

template<typename K, typename V>
class linear_hashmap
{
public:
    using key_t = K;
    using value_t = V;
    using hash32_fn = std::function<uint32_t(const key_t &)>;
    static_assert(std::is_default_constructible_v<key_t>, "key_t not default-constructible");
    static_assert(std::is_default_constructible_v<value_t>, "value_t not default-constructible");
    static_assert(std::equality_comparable<key_t>, "key_t not comparable");

    linear_hashmap() = default;
    linear_hashmap(const linear_hashmap &) = delete;
    linear_hashmap(linear_hashmap &other) : linear_hashmap() { swap(*this, other); };
    linear_hashmap &operator=(linear_hashmap other)
    {
        swap(*this, other);
        return *this;
    }

    explicit linear_hashmap(uint64_t min_capacity)
        : m_capacity(crocore::next_pow_2(min_capacity)), m_storage(std::make_unique<storage_item_t[]>(m_capacity))
    {
        clear();
    }

    inline size_t size() const { return m_num_elements; }

    inline size_t capacity() const { return m_capacity; }

    inline bool empty() const { return size() == 0; }

    inline void clear()
    {
        std::unique_lock lock(m_mutex);
        m_num_elements = 0;
        storage_item_t *ptr = m_storage.get(), *end = ptr + m_capacity;
        for(; ptr != end; ++ptr)
        {
            ptr->key = key_t();
            ptr->value = std::optional<value_t>();
        }
    }

    uint32_t put(const key_t &key, const value_t &value)
    {
        check_load_factor();
        return internal_put(key, value);
    }

    [[nodiscard]] std::optional<value_t> get(const key_t &key) const
    {
        if(!m_capacity) { return {}; }
        std::shared_lock lock(m_mutex);

        for(uint32_t idx = m_hash_fn(key);; idx++)
        {
            idx &= m_capacity - 1;
            auto &item = m_storage[idx];
            if(item.key == key_t()) { return {}; }
            else if(key == item.key)
            {
                if(auto value = item.value.load()) { return value; }
            }
        }
    }

    void remove(const key_t &key)
    {
        if(!m_capacity) { return; }
        std::shared_lock lock(m_mutex);

        for(uint32_t idx = m_hash_fn(key);; idx++)
        {
            idx &= m_capacity - 1;
            auto &item = m_storage[idx];
            if(item.key == key_t()) { return; }
            else if(key == item.key && item.value.load())
            {
                item.value = std::optional<value_t>();
                m_num_elements--;
                return;
            }
        }
    }

    [[nodiscard]] inline bool contains(const key_t &key) const { return get(key) != std::nullopt; }

    size_t get_storage(void *dst) const
    {
        if(dst)
        {
            std::unique_lock lock(m_mutex);

            struct output_item_t
            {
                key_t key = {};
                value_t value = {};
            };
            auto output_ptr = reinterpret_cast<output_item_t *>(dst);
            storage_item_t *item = m_storage.get(), *end = item + m_capacity;
            for(; item != end; ++item, ++output_ptr)
            {
                if(item->key != key_t())
                {
                    output_ptr->key = item->key;
                    auto value = item->value.load();
                    output_ptr->value = value ? *value : value_t();
                }
                else { *output_ptr = {}; }
            }
        }
        return (sizeof(key_t) + sizeof(value_t)) * m_capacity;
    }

    void reserve(size_t new_capacity)
    {
        new_capacity = crocore::next_pow_2(std::max<size_t>(m_num_elements, new_capacity));
        auto new_linear_hashmap = linear_hashmap(new_capacity);
        storage_item_t *ptr = m_storage.get(), *end = ptr + m_capacity;
        for(; ptr != end; ++ptr)
        {
            if(ptr->key != key_t())
            {
                if(auto value = ptr->value.load()) { new_linear_hashmap.put(ptr->key, *value); }
            }
        }
        swap(*this, new_linear_hashmap);
    }

    float load_factor() const { return static_cast<float>(m_num_elements) / m_capacity; }

    float max_load_factor() const { return m_max_load_factor; }

    void max_load_factor(float load_factor)
    {
        m_max_load_factor = std::clamp<float>(load_factor, 0.01f, 1.f);
        check_load_factor();
    }

    friend void swap(linear_hashmap &lhs, linear_hashmap &rhs)
    {
        std::lock(lhs.m_mutex, rhs.m_mutex);
        std::unique_lock lock_lhs(lhs.m_mutex, std::adopt_lock), lock_rhs(rhs.m_mutex, std::adopt_lock);
        std::swap(lhs.m_capacity, rhs.m_capacity);
        lhs.m_num_elements = rhs.m_num_elements.exchange(lhs.m_num_elements);
        std::swap(lhs.m_storage, rhs.m_storage);
        std::swap(lhs.m_hash_fn, rhs.m_hash_fn);
        std::swap(lhs.m_max_load_factor, rhs.m_max_load_factor);
        std::swap(lhs.m_grow_factor, rhs.m_grow_factor);
    }

private:
    struct storage_item_t
    {
        std::atomic<key_t> key;
        std::atomic<std::optional<value_t>> value;
    };

    inline void check_load_factor()
    {
        if(m_num_elements >= m_capacity * m_max_load_factor)
        {
            reserve(std::max<size_t>(32, static_cast<size_t>(m_grow_factor * m_capacity)));
        }
    }

    inline uint32_t internal_put(const key_t key, const value_t &value)
    {
        std::shared_lock lock(m_mutex);
        uint32_t probe_length = 0;

        for(uint64_t idx = m_hash_fn(key);; idx++, probe_length++)
        {
            idx &= m_capacity - 1;
            auto &item = m_storage[idx];

            // load previous key
            key_t probed_key = m_storage[idx].key;

            if(probed_key != key)
            {
                // hit another valid entry, keep probing
                if(probed_key != key_t() && item.value.load()) { continue; }

                item.key.compare_exchange_strong(probed_key, key, std::memory_order_relaxed, std::memory_order_relaxed);
                if((probed_key != key_t()) && (probed_key != key))
                {
                    // another thread just stole it
                    continue;
                }
                m_num_elements++;
            }
            item.value = value;
            return probe_length;
        }
    }

    uint64_t m_capacity{};
    std::atomic<uint64_t> m_num_elements;
    std::unique_ptr<storage_item_t[]> m_storage;
    hash32_fn m_hash_fn = std::bind(vierkant::murmur3_32<key_t>, std::placeholders::_1, 0);
    mutable std::shared_mutex m_mutex;

    // reasonably low load-factor to keep average probe-lengths low
    float m_max_load_factor = 0.5f;
    float m_grow_factor = 2.f;
};
}// namespace vierkant

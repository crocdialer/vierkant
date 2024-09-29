#include <gtest/gtest.h>
#include <vierkant/linear_hashmap.hpp>


TEST(linear_hashmap, empty)
{
    vierkant::linear_hashmap<uint64_t, uint64_t> hashmap;
    EXPECT_TRUE(hashmap.empty());
    EXPECT_EQ(hashmap.capacity(), 0);
    EXPECT_FALSE(hashmap.storage_num_bytes());
}

TEST(linear_hashmap, basic)
{
    constexpr uint32_t test_capacity = 100;
    vierkant::linear_hashmap<uint64_t, uint64_t> hashmap(test_capacity);
    EXPECT_TRUE(hashmap.empty());
    EXPECT_TRUE(hashmap.storage_num_bytes());

    // capacity will be rounded to next pow2
    EXPECT_GE(hashmap.capacity(), test_capacity);
    EXPECT_TRUE(crocore::is_pow_2(hashmap.capacity()));

    EXPECT_FALSE(hashmap.contains(13));
    EXPECT_FALSE(hashmap.contains(42));

    auto &v1 = hashmap.put(69, 99);
    EXPECT_EQ(v1, 99);
    auto &v2 = hashmap.put(13, 12);
    EXPECT_EQ(v2, 12);
    EXPECT_EQ(hashmap.size(), 2);

    EXPECT_TRUE(hashmap.contains(69));
    EXPECT_EQ(hashmap.get(69), 99);
    EXPECT_TRUE(hashmap.contains(13));
    EXPECT_EQ(hashmap.get(13), 12);

    auto storage = std::make_unique<uint8_t[]>(hashmap.storage_num_bytes());
    hashmap.get_storage(storage.get());
}

TEST(linear_hashmap, custom_key)
{
    // custom 32-byte key
    struct custom_key_t
    {
        int v[8]{};
        constexpr bool operator==(const custom_key_t &other) const
        {
            for(uint32_t i = 0; i < 8; ++i)
            {
                if(v[i] != other.v[i]) { return false; }
            }
            return true;
        }
    };
    constexpr uint32_t test_capacity = 100;
    auto hashmap = vierkant::linear_hashmap<custom_key_t, uint64_t>(test_capacity);

    custom_key_t k1{{1, 2, 3, 4, 5, 6, 7, 8}};
    hashmap.put(k1, 69);
    EXPECT_TRUE(hashmap.contains(k1));
    EXPECT_FALSE(hashmap.contains(custom_key_t()));
}

TEST(linear_hashmap, resize)
{
    vierkant::linear_hashmap<uint64_t, uint64_t> hashmap;

    // empty / no capacity specified -> expect overflow on insert
    EXPECT_THROW(hashmap.put(13, 12), std::overflow_error);

    // fix by resizing
    hashmap.resize(17);
    EXPECT_TRUE(hashmap.empty());
    hashmap.put(13, 12);
    EXPECT_TRUE(hashmap.contains(13));
}
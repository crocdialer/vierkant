#include <vierkant/linear_hashmap.hpp>
#include <gtest/gtest.h>


TEST(linear_hashmap, empty)
{
    vierkant::linear_hashmap hashmap;
    EXPECT_TRUE(hashmap.empty());
    EXPECT_EQ(hashmap.capacity(), 0);
    EXPECT_EQ(hashmap.storage(), nullptr);
    EXPECT_FALSE(hashmap.storage_num_bytes());

    // empty / no capacity specified -> expect overflow on insert
    EXPECT_THROW(hashmap.insert(13, 12), std::overflow_error);
}

TEST(linear_hashmap, basic)
{
    constexpr uint32_t test_capacity = 100;
    vierkant::linear_hashmap hashmap(test_capacity);
    EXPECT_TRUE(hashmap.empty());
    EXPECT_TRUE(hashmap.storage());
    EXPECT_TRUE(hashmap.storage_num_bytes());

    // capacity will be rounded to next pow2
    EXPECT_GE(hashmap.capacity(), test_capacity);
    EXPECT_TRUE(crocore::is_pow_2(hashmap.capacity()));

    EXPECT_FALSE(hashmap.contains(13));
    EXPECT_FALSE(hashmap.contains(42));

    hashmap.insert(69, 99);
    hashmap.insert(13, 12);
    EXPECT_EQ(hashmap.size(), 2);

    EXPECT_TRUE(hashmap.contains(69));
    EXPECT_EQ(hashmap.get(69), 99);
    EXPECT_TRUE(hashmap.contains(13));
    EXPECT_EQ(hashmap.get(13), 12);
}
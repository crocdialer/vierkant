module integer_traits;

import hash;

namespace utils
{

public interface INullable
{
    static This zero();
}

public interface IHashable
{
    uint hash();
}


extension int : utils::INullable, utils::IHashable
{
    static int zero() { return 0; }
    uint hash() { return utils::xxhash32(this, 0); }
}

extension uint : utils::INullable, utils::IHashable
{
    static uint zero() { return 0u; }
    uint hash() { return utils::xxhash32(this, 0); }
}

extension int16_t : utils::INullable, utils::IHashable
{
    static int16_t zero() { return int16_t(0); }
    uint hash() { return utils::xxhash32(this, 0); }
}

extension uint16_t : utils::INullable, utils::IHashable
{
    static uint16_t zero() { return uint16_t(0u); }
    uint hash() { return utils::xxhash32(this, 0); }
}

extension int64_t : utils::INullable, utils::IHashable
{
    static int64_t zero() { return int64_t(0); }
    bool equals(int64_t other) { return this == other; }
    // fold the two halves together
    uint hash() { return utils::xxhash32(uint(this >> 32), utils::xxhash32(uint(this & 0xFFFFFFFF), 0)); }
}

extension uint64_t : utils::INullable, utils::IHashable
{
    static uint64_t zero() { return uint64_t(0u); }
    uint hash() { return utils::xxhash32(uint(this >> 32), utils::xxhash32(uint(this & 0xFFFFFFFF), 0)); }
}

}

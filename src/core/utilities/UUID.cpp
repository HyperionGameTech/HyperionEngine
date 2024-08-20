#include <core/utilities/UUID.hpp>
#include <core/containers/String.hpp>

#include <core/threading/Threads.hpp>

#include <core/system/Time.hpp>

#include <Types.hpp>

#include <random>

namespace hyperion {
namespace utilities {

static uint64 RandomNumber()
{
    static thread_local std::mt19937 random_engine(uint32(uint64(Threads::CurrentThreadID().value) + uint64(Time::Now())));
    std::uniform_int_distribution<uint64> distribution;

    return distribution(random_engine);
}

UUID::UUID(UUIDVersion version)
    : data0 { 0 },
      data1 { 0 }
{
    switch (version) {
    case UUIDVersion::UUIDv4:
        data0 = RandomNumber();
        data1 = RandomNumber();

        data0 &= ~0xF000;
        data0 |= 0x4000;
        data1 &= ~0xC000000000000000;
        data1 |= 0x8000000000000000;

        break;
    default:
        AssertThrowMsg(false, "Invalid UUID version");
    }
}

ANSIString UUID::ToString() const
{
    union {
        uint64  data[2];
        uint8   bytes[16];
    } u {
        IsLittleEndian() ? data0 : SwapEndianness(data0),
        IsLittleEndian() ? data1 : SwapEndianness(data1)
    };

    char buffer[37] = { '\0' };

    std::snprintf(buffer, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", 
        u.bytes[7], u.bytes[6], u.bytes[5], u.bytes[4], u.bytes[3], u.bytes[2], u.bytes[1], u.bytes[0],
        u.bytes[15], u.bytes[14], u.bytes[13], u.bytes[12], u.bytes[11], u.bytes[10], u.bytes[9], u.bytes[8]
    );

    return ANSIString(buffer);
}

} // namespace utilities
} // namespace hyperion
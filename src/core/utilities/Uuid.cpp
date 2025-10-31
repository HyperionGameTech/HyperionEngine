#include <core/utilities/Uuid.hpp>
#include <core/containers/String.hpp>

#include <core/threading/Threads.hpp>

#include <core/utilities/Time.hpp>
#include <core/utilities/ByteUtil.hpp>

#include <core/Types.hpp>

#include <random>

#ifndef HYP_BUILDTOOL
#include <Uuid.generated.inl>
#endif

namespace hyperion {
namespace utilities {

static uint64 RandomNumber()
{
    static thread_local std::mt19937 s_randomEngine(uint32(uint64(ThreadId::Current().GetValue()) + uint64(Time::Now())));
    std::uniform_int_distribution<uint64> distribution;

    return distribution(s_randomEngine);
}

Uuid::Uuid()
    : data0 { RandomNumber() },
      data1 { RandomNumber() }
{
    data0 &= ~0xF000;
    data0 |= 0x4000;
    data1 &= ~0xC000000000000000;
    data1 |= 0x8000000000000000;
}

Uuid::Uuid(const char* str)
{
    SizeType len = Memory::StrLen(str);

    if (!len) // null or zero length
    {
        *this = Invalid();

        return;
    }

    if (len > 36)
    {
        len = 36;
    }

    union
    {
        uint64 data[2];
        uint8 bytes[16];
    };

    char buffer[37] {};
    Memory::MemCpy(buffer, str, len < 36 ? len : 36);
    buffer[36] = '\0';

    unsigned int uints[16];

    std::sscanf(buffer, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        &uints[7], &uints[6], &uints[5], &uints[4], &uints[3], &uints[2], &uints[1], &uints[0],
        &uints[15], &uints[14], &uints[13], &uints[12], &uints[11], &uints[10], &uints[9], &uints[8]);

    for (int i = 0; i < 16; i++)
    {
        bytes[i] = (uint8)uints[i];
    }

    data0 = data[0];
    data1 = data[1];
}

String Uuid::ToString() const
{
    union
    {
        uint64 data[2];
        uint8 bytes[16];
    } u {
        ByteUtil::IsLittleEndian() ? data0 : SwapEndian(data0),
        ByteUtil::IsLittleEndian() ? data1 : SwapEndian(data1)
    };

    char buffer[37] = { '\0' };

    std::snprintf(buffer, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        u.bytes[7], u.bytes[6], u.bytes[5], u.bytes[4], u.bytes[3], u.bytes[2], u.bytes[1], u.bytes[0],
        u.bytes[15], u.bytes[14], u.bytes[13], u.bytes[12], u.bytes[11], u.bytes[10], u.bytes[9], u.bytes[8]);

    return String(buffer);
}

} // namespace utilities
} // namespace hyperion

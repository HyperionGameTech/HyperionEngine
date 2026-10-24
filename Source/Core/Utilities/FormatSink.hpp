/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <type_traits>

namespace Hyperion {

class ByteWriter;

namespace utilities {

// A sink is either a ByteWriter (Write and Position) or a string (Append and Size)
template <class OutType>
static inline void FormatInto_Write(OutType& out, const char* begin, size_t size)
{
    if constexpr (std::is_base_of_v<ByteWriter, OutType>)
    {
        out.Write(begin, size);
    }
    else
    {
        out.Append(begin, begin + size);
    }
}

template <class OutType>
static inline void FormatInto_WriteString(OutType& out, const char* str)
{
    FormatInto_Write(out, str, std::strlen(str));
}

template <class OutType>
static inline size_t FormatInto_Size(const OutType& out)
{
    if constexpr (std::is_base_of_v<ByteWriter, OutType>)
    {
        return out.Position();
    }
    else
    {
        return out.Size();
    }
}

// Writes one scalar with printf semantics. 512 bytes holds the longest %f output of any double, so nothing is truncated.
template <class OutType, class T>
static inline void FormatInto_Scalar(OutType& out, T value)
{
    char buffer[512];
    int size = 0;

    if constexpr (std::is_floating_point_v<T>)
    {
        size = std::snprintf(buffer, sizeof(buffer), "%f", double(value));
    }
    else if constexpr (std::is_signed_v<T>)
    {
        size = std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
    }
    else
    {
        size = std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
    }

    FormatInto_Write(out, buffer, size_t(size));
}

} // namespace utilities
} // namespace Hyperion

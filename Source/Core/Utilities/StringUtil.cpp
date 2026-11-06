/*!
 *  @author The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <charconv>

#include <Core/Utilities/StringUtil.hpp>

#include <cstring>
#include <limits>
#include <type_traits>

namespace Hyperion {
namespace StringUtil {

namespace {

bool IsSpace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

void TrimSpace(const char*& begin, const char*& end)
{
    while (begin != end && IsSpace(*begin))
    {
        ++begin;
    }

    while (end != begin && IsSpace(end[-1]))
    {
        --end;
    }
}

template <class T>
bool ParseInteger(const char* begin, const char* end, T* outValue)
{
    TrimSpace(begin, end);

    bool isNegative = false;

    if (begin != end && (*begin == '-' || *begin == '+'))
    {
        isNegative = *begin == '-';
        ++begin;
    }

    int base = 10;

    if (end - begin >= 2 && begin[0] == '0' && (begin[1] == 'x' || begin[1] == 'X'))
    {
        begin += 2;
        base = 16;
    }

    uint64 magnitude = 0;
    const std::from_chars_result result = std::from_chars(begin, end, magnitude, base);

    if (result.ec != std::errc() || result.ptr != end)
    {
        return false;
    }

    if constexpr (std::is_signed_v<T>)
    {
        uint64 limit = uint64(std::numeric_limits<T>::max());

        if (isNegative)
        {
            limit += 1;
        }

        if (magnitude > limit)
        {
            return false;
        }

        if (isNegative)
        {
            *outValue = T(uint64(0) - magnitude);
        }
        else
        {
            *outValue = T(magnitude);
        }
    }
    else
    {
        if ((isNegative && magnitude != 0) || magnitude > uint64(std::numeric_limits<T>::max()))
        {
            return false;
        }

        *outValue = T(magnitude);
    }

    return true;
}

template <class T>
bool ParseFloat(const char* begin, const char* end, T* outValue)
{
    TrimSpace(begin, end);

    // from_chars takes no leading '+'
    if (begin != end && *begin == '+')
    {
        ++begin;
    }

    if (begin == end)
    {
        return false;
    }

#if defined(__cpp_lib_to_chars)
    T value {};
    const std::from_chars_result result = std::from_chars(begin, end, value);

    if (result.ec != std::errc() || result.ptr != end)
    {
        return false;
    }

    *outValue = value;

    return true;
#else
    // libc++ before 20 has no floating point from_chars, and strtod needs a terminator
    char buffer[128];
    const size_t size = size_t(end - begin);

    if (size >= sizeof(buffer))
    {
        return false;
    }

    std::memcpy(buffer, begin, size);
    buffer[size] = '\0';

    char* parsedEnd = nullptr;
    const double value = std::strtod(buffer, &parsedEnd);

    if (parsedEnd != buffer + size)
    {
        return false;
    }

    *outValue = T(value);

    return true;
#endif
}

} // namespace

bool ParseNumber(const char* begin, const char* end, int8* outValue)
{
    return ParseInteger(begin, end, outValue);
}

bool ParseNumber(const char* begin, const char* end, int16* outValue)
{
    return ParseInteger(begin, end, outValue);
}

bool ParseNumber(const char* begin, const char* end, int32* outValue)
{
    return ParseInteger(begin, end, outValue);
}

bool ParseNumber(const char* begin, const char* end, int64* outValue)
{
    return ParseInteger(begin, end, outValue);
}

bool ParseNumber(const char* begin, const char* end, uint8* outValue)
{
    return ParseInteger(begin, end, outValue);
}

bool ParseNumber(const char* begin, const char* end, uint16* outValue)
{
    return ParseInteger(begin, end, outValue);
}

bool ParseNumber(const char* begin, const char* end, uint32* outValue)
{
    return ParseInteger(begin, end, outValue);
}

bool ParseNumber(const char* begin, const char* end, uint64* outValue)
{
    return ParseInteger(begin, end, outValue);
}

bool ParseNumber(const char* begin, const char* end, float* outValue)
{
    return ParseFloat(begin, end, outValue);
}

bool ParseNumber(const char* begin, const char* end, double* outValue)
{
    return ParseFloat(begin, end, outValue);
}

} // namespace StringUtil
} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/utilities/Time.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ObjectMacros.hpp>

#ifdef HYP_UNIX
#include <sys/time.h>
#elif defined(HYP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

namespace Hyperion {
namespace utilities {

CORE_API const Class* g_clsTime = nullptr;

#pragma region TimeDiff

TimeDiff TimeDiff::operator+(const Time& other) const
{
    return TimeDiff(milliseconds + other.m_value);
}

TimeDiff& TimeDiff::operator+=(const Time& other)
{
    milliseconds += other.m_value;
    return *this;
}

TimeDiff TimeDiff::operator-(const Time& other) const
{
    return TimeDiff(milliseconds + other.m_value);
}

TimeDiff& TimeDiff::operator-=(const Time& other)
{
    milliseconds += other.m_value;
    return *this;
}

#pragma endregion TimeDiff

#pragma region Time

Time::Time()
    : m_value(Time::Now().m_value)
{
}

Time::Time(uint64 timestamp)
    : m_value(timestamp)
{
}

Time Time::Now()
{
#ifdef HYP_UNIX
    struct timeval tv;
    gettimeofday(&tv, 0);

    // convert to milliseconds
    return Time(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#elif defined(HYP_WINDOWS)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    // convert to milliseconds
    return Time((uint64(ft.dwHighDateTime) << 32 | ft.dwLowDateTime) / 10000);
#endif
}

#pragma endregion Time

} // namespace utilities
} // namespace Hyperion

using namespace Hyperion;

using Time = Hyperion::utilities::Time;
using Hyperion::Class;

static const Class*& g_clsTime = Hyperion::utilities::g_clsTime;

// clang-format off
HYP_BEGIN_STRUCT(Time, -1, 0, {})
    Field(NAME(HYP_STR(m_value)), &Type::m_value, HYP_OFFSET_OF(Type, m_value))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Time);

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Types.hpp>

#include <thread>
#include <type_traits>

namespace hyperion {
namespace threading {

constexpr uint32 g_threadDynamicMask = ~(~0u >> 1); // last bit
constexpr uint32 g_threadCategoryMask = ~(~0u << 4);
constexpr uint32 g_threadIdMask = ~(g_threadCategoryMask | g_threadDynamicMask);
constexpr uint32 g_maxStaticThreadIds = uint32(MathUtil::FastLog2_Pow2((~0u & g_threadIdMask) >> 4));

using ThreadMask = uint32;

enum ThreadCategory : ThreadMask;

class ThreadId
{
public:
    enum AllocateFlags : uint32
    {
        NONE = 0x0,
        DYNAMIC = 0x1,
        FORCE_UNIQUE = 0x2
    };

    static const ThreadId invalid;

    HYP_API static const ThreadId& Current();
    HYP_API static const ThreadId& Invalid();

    constexpr ThreadId()
        : m_value(0)
    {
    }

    HYP_API ThreadId(Name name, bool forceUnique = false);
    HYP_API ThreadId(Name name, ThreadCategory category, bool forceUnique = false);

    constexpr ThreadId(const ThreadId& other) = default;
    ThreadId& operator=(const ThreadId& other) = default;
    constexpr ThreadId(ThreadId&& other) noexcept = default;
    ThreadId& operator=(ThreadId&& other) noexcept = default;
    ~ThreadId() = default;

    HYP_FORCE_INLINE constexpr bool operator==(const ThreadId& other) const
    {
        return m_value == other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const ThreadId& other) const
    {
        return m_value != other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const ThreadId& other) const
    {
        return m_value < other.m_value;
    }

    HYP_FORCE_INLINE constexpr Name GetName() const
    {
        return m_name;
    }

    /*! \brief Check if this thread Id is a dynamic thread Id.
     *  \returns True if this is a dynamic thread Id, false otherwise. */
    HYP_FORCE_INLINE constexpr bool IsDynamic() const
    {
        return (m_value & g_threadDynamicMask) >> 31;
    }

    HYP_FORCE_INLINE constexpr bool IsStatic() const
    {
        return !((m_value & g_threadDynamicMask) >> 31);
    }

    HYP_FORCE_INLINE constexpr ThreadCategory GetCategory() const
    {
        return ThreadCategory(m_value & g_threadCategoryMask);
    }

    HYP_FORCE_INLINE constexpr uint32 GetValue() const
    {
        return m_value;
    }

    /*! \brief Returns a mask value that can be used to match against ThreadIds that meet the same criteria as this ThreadId.
     *  For static thread IDs, this matches the same value / bits.
     *  For dyanmic thread IDs, the dynamic bit and thread category are preserved but the actual value is not */
    HYP_FORCE_INLINE constexpr ThreadMask GetMask() const
    {
        return IsDynamic() ? (m_value & ~g_threadIdMask) : m_value;
    }

    HYP_FORCE_INLINE constexpr bool IsValid() const
    {
        return m_value != 0;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(m_value);
    }

protected:
    HYP_API ThreadId(Name name, ThreadCategory category, uint32 allocateFlags);

    uint32 m_value;
    Name m_name;
};

static_assert(std::is_trivially_destructible_v<ThreadId>,
    "ThreadId must be trivially destructible! Otherwise thread_local current_thread_id var may  be generated using a wrapper function.");

/*! \brief StaticThreadIds may be used for bitwise operations as only one bit will be set for the value. */
class HYP_API StaticThreadId : public ThreadId
{
public:
    friend constexpr ThreadMask operator|(ThreadMask lhs, const StaticThreadId& rhs);
    friend constexpr ThreadMask operator&(ThreadMask lhs, const StaticThreadId& rhs);

    constexpr StaticThreadId() = default;

    /*! \brief Allocate a new StaticThreadId with the given Name.
     *  \param name The name to use to allocate the ThreadId.
     *  \param forceUnique If true, a new index will be allocated for the ThreadId regardless of whether or not one already exists with the given name. */
    explicit StaticThreadId(Name name, bool forceUnique = false);

    /*! \brief Construct a StaticThreadId from an pre-allocated static thread Id index */
    explicit StaticThreadId(uint32 staticThreadIndex);

    constexpr StaticThreadId(const StaticThreadId& other) = default;
    StaticThreadId& operator=(const StaticThreadId& other) = default;
    constexpr StaticThreadId(StaticThreadId&& other) noexcept = default;
    StaticThreadId& operator=(StaticThreadId&& other) noexcept = default;
    ~StaticThreadId() = default;

    /*! \brief StaticThreadId can be converted to ThreadMask directly since only one bit will be set */
    HYP_FORCE_INLINE constexpr explicit operator ThreadMask() const
    {
        return m_value;
    }

    HYP_FORCE_INLINE constexpr ThreadMask operator~() const
    {
        return ~m_value;
    }

    HYP_FORCE_INLINE constexpr ThreadMask operator|(const StaticThreadId& other) const
    {
        return m_value | other.m_value;
    }

    HYP_FORCE_INLINE constexpr ThreadMask operator&(const StaticThreadId& other) const
    {
        return m_value & other.m_value;
    }

    HYP_FORCE_INLINE constexpr ThreadMask operator|(ThreadMask mask) const
    {
        return m_value | mask;
    }

    HYP_FORCE_INLINE constexpr ThreadMask operator&(ThreadMask mask) const
    {
        return m_value & mask;
    }

    HYP_FORCE_INLINE constexpr uint32 GetStaticThreadIndex() const
    {
        return uint32(MathUtil::FastLog2_Pow2((m_value & g_threadIdMask) >> 4));
    }
};

HYP_FORCE_INLINE constexpr ThreadMask operator|(ThreadMask lhs, const StaticThreadId& rhs)
{
    return lhs | rhs.m_value;
}

HYP_FORCE_INLINE constexpr ThreadMask operator&(ThreadMask lhs, const StaticThreadId& rhs)
{
    return lhs & rhs.m_value;
}

static_assert(sizeof(StaticThreadId) == sizeof(ThreadId), "StaticThreadId must not increase in size relative to base class ThreadId");

} // namespace threading

using threading::StaticThreadId;
using threading::ThreadId;
using threading::ThreadMask;

} // namespace hyperion

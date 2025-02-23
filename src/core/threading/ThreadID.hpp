/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_THREAD_ID_HPP
#define HYPERION_THREAD_ID_HPP

#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/math/MathUtil.hpp>

#include <Types.hpp>

#include <thread>
#include <type_traits>

namespace hyperion {
namespace threading {

constexpr uint32 g_thread_dynamic_mask = ~(~0u >> 1); // last bit
constexpr uint32 g_thread_category_mask = ~(~0u << 4);
constexpr uint32 g_thread_id_mask = ~(g_thread_category_mask | g_thread_dynamic_mask);

constexpr uint32 g_max_static_thread_ids = MathUtil::FastLog2_Pow2((~0u & g_thread_id_mask) >> 4);

using ThreadMask = uint32;

enum ThreadCategory : ThreadMask;

class ThreadID
{
public:
    enum AllocateFlags : uint32
    {
        NONE            = 0x0,
        DYNAMIC         = 0x1,
        FORCE_UNIQUE    = 0x2
    };

    static const ThreadID invalid;

    HYP_API static const ThreadID &Current();
    HYP_API static const ThreadID &Invalid();

    constexpr ThreadID()
        : m_value(0)
    {
    }

    HYP_API ThreadID(Name name, bool force_unique = false);
    HYP_API ThreadID(Name name, ThreadCategory category, bool force_unique = false);

    constexpr ThreadID(const ThreadID &other)       = default;
    ThreadID &operator=(const ThreadID &other)      = default;
    constexpr ThreadID(ThreadID &&other) noexcept   = default;
    ThreadID &operator=(ThreadID &&other) noexcept  = default;
    ~ThreadID()                                     = default;

    HYP_FORCE_INLINE constexpr bool operator==(const ThreadID &other) const
        { return m_value == other.m_value; }

    HYP_FORCE_INLINE constexpr bool operator!=(const ThreadID &other) const
        { return m_value != other.m_value; }

    HYP_FORCE_INLINE constexpr bool operator<(const ThreadID &other) const
        { return m_value < other.m_value; }
    
    HYP_FORCE_INLINE constexpr Name GetName() const
        { return m_name; }

    /*! \brief Check if this thread ID is a dynamic thread ID.
     *  \returns True if this is a dynamic thread ID, false otherwise. */
    HYP_FORCE_INLINE constexpr bool IsDynamic() const
        { return (m_value & g_thread_dynamic_mask) >> 31; }

    HYP_FORCE_INLINE constexpr bool IsStatic() const
        { return !((m_value & g_thread_dynamic_mask) >> 31); }

    HYP_FORCE_INLINE constexpr ThreadCategory GetCategory() const
        { return ThreadCategory(m_value & g_thread_category_mask); }

    HYP_FORCE_INLINE constexpr uint32 GetValue() const
        { return m_value; }

    /*! \brief Returns a mask value that can be used to match against ThreadIDs that meet the same criteria as this ThreadID.
     *  For static thread IDs, this matches the same value / bits.
     *  For dyanmic thread IDs, the dynamic bit and thread category are preserved but the actual value is not */
    HYP_FORCE_INLINE constexpr ThreadMask GetMask() const
        { return IsDynamic() ? (m_value & ~g_thread_id_mask) : m_value; }

    HYP_FORCE_INLINE constexpr bool IsValid() const
        { return m_value != 0; }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
        { return HashCode::GetHashCode(m_value); }

protected:
    HYP_API ThreadID(Name name, ThreadCategory category, uint32 allocate_flags);

    uint32  m_value;
    Name    m_name;
};

static_assert(std::is_trivially_destructible_v<ThreadID>,
    "ThreadID must be trivially destructible! Otherwise thread_local current_thread_id var may  be generated using a wrapper function.");

/*! \brief StaticThreadIDs may be used for bitwise operations as only one bit will be set for the value. */
class HYP_API StaticThreadID : public ThreadID
{
public:
    friend constexpr ThreadMask operator|(ThreadMask lhs, const StaticThreadID &rhs);
    friend constexpr ThreadMask operator&(ThreadMask lhs, const StaticThreadID &rhs);

    constexpr StaticThreadID()                                  = default;

    /*! \brief Allocate a new StaticThreadID with the given Name.
     *  \param name The name to use to allocate the ThreadID.
     *  \param force_unique If true, a new index will be allocated for the ThreadID regardless of whether or not one already exists with the given name. */
    explicit StaticThreadID(Name name, bool force_unique = false);

    /*! \brief Construct a StaticThreadID from an pre-allocated static thread ID index */
    explicit StaticThreadID(uint32 static_thread_index);

    constexpr StaticThreadID(const StaticThreadID &other)       = default;
    StaticThreadID &operator=(const StaticThreadID &other)      = default;
    constexpr StaticThreadID(StaticThreadID &&other) noexcept   = default;
    StaticThreadID &operator=(StaticThreadID &&other) noexcept  = default;
    ~StaticThreadID()                                           = default;

    /*! \brief StaticThreadID can be converted to ThreadMask directly since only one bit will be set */
    HYP_FORCE_INLINE constexpr explicit operator ThreadMask() const
        { return m_value; }

    HYP_FORCE_INLINE constexpr ThreadMask operator~() const
        { return ~m_value; }
    
    HYP_FORCE_INLINE constexpr ThreadMask operator|(const StaticThreadID &other) const
        { return m_value | other.m_value; }

    HYP_FORCE_INLINE constexpr ThreadMask operator&(const StaticThreadID &other) const
        { return m_value & other.m_value; }

    HYP_FORCE_INLINE constexpr ThreadMask operator|(ThreadMask mask) const
        { return m_value | mask; }

    HYP_FORCE_INLINE constexpr ThreadMask operator&(ThreadMask mask) const
        { return m_value & mask; }

    HYP_FORCE_INLINE constexpr uint32 GetStaticThreadIndex() const
        { return MathUtil::FastLog2_Pow2((m_value & g_thread_id_mask) >> 4); }
};

HYP_FORCE_INLINE constexpr ThreadMask operator|(ThreadMask lhs, const StaticThreadID &rhs)
{
    return lhs | rhs.m_value;
}

HYP_FORCE_INLINE constexpr ThreadMask operator&(ThreadMask lhs, const StaticThreadID &rhs)
{
    return lhs & rhs.m_value;
}

static_assert(sizeof(StaticThreadID) == sizeof(ThreadID), "StaticThreadID must not increase in size relative to base class ThreadID");

} // namespace threading

using threading::ThreadMask;
using threading::ThreadID;
using threading::StaticThreadID;

} // namespace hyperion

#endif
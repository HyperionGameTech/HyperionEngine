/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/ThreadId.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Mutex.hpp>

#include <core/containers/HashMap.hpp>

#include <core/utilities/GlobalContext.hpp>

#include <core/IdGenerator.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace threading {

const ThreadId ThreadId::invalid = ThreadId();

class GlobalThreadIdCache
{
public:
    GlobalThreadIdCache()
    {
    }

    ~GlobalThreadIdCache()
    {
    }

    Name FindNameByIndex(uint32 index)
    {
        Mutex::Guard guard(m_mutex);

        auto it = m_reverse_name_mapping.Find(index);

        if (it == m_reverse_name_mapping.End())
        {
            return Name();
        }

        return it->second;
    }

    uint32 AllocateIndex(Name name)
    {
        const uint32 index = m_id_generator.NextID();

        {
            Mutex::Guard guard(m_mutex);

            m_name_mapping[name].PushBack(index);
            m_reverse_name_mapping[index] = name;
        }

        return index;
    }

    uint32 FindOrAllocateIndex(Name name)
    {
        Mutex::Guard guard(m_mutex);

        if (uint32* index_ptr = FindIndexByName_Internal(name))
        {
            return *index_ptr;
        }

        const uint32 index = m_id_generator.NextID();

        m_name_mapping[name].PushBack(index);
        m_reverse_name_mapping[index] = name;

        return index;
    }

private:
    uint32* FindIndexByName_Internal(Name name)
    {
        auto it = m_name_mapping.Find(name);

        if (it != m_name_mapping.End() && it->second.Any())
        {
            return &it->second.Front();
        }

        return nullptr;
    }

    IdGenerator m_id_generator;

    HashMap<Name, Array<uint32>> m_name_mapping;
    HashMap<uint32, Name> m_reverse_name_mapping;

    mutable Mutex m_mutex;
};

static GlobalThreadIdCache& GetStaticThreadIdCache()
{
    static GlobalThreadIdCache cache;

    return cache;
}

static GlobalThreadIdCache& GetDynamicThreadIdCache()
{
    static GlobalThreadIdCache cache;

    return cache;
}

#pragma region ThreadId

static uint32 AllocateThreadId(Name name, uint32 allocate_flags)
{
    uint32 thread_id_value;

    if (allocate_flags & ThreadId::AllocateFlags::DYNAMIC)
    {
        thread_id_value = (allocate_flags & ThreadId::AllocateFlags::FORCE_UNIQUE)
            ? GetDynamicThreadIdCache().AllocateIndex(name)
            : GetDynamicThreadIdCache().FindOrAllocateIndex(name);
    }
    else
    {
        thread_id_value = (allocate_flags & ThreadId::AllocateFlags::FORCE_UNIQUE)
            ? GetStaticThreadIdCache().AllocateIndex(name)
            : GetStaticThreadIdCache().FindOrAllocateIndex(name);

        AssertThrowMsg(thread_id_value < g_max_static_thread_ids, "Maximum static thread id value exceeded!");

        thread_id_value = 1u << (thread_id_value - 1);
    }

    AssertThrowMsg((((thread_id_value << 4) & g_thread_id_mask) >> 4) == thread_id_value,
        "Thread Id value %u exceeds maximum value!",
        thread_id_value);

    return thread_id_value;
}

static uint32 MakeThreadIdValue(Name name, ThreadCategory category, uint32 allocate_flags)
{
    uint32 value = 0;

    value |= (uint32(category) & g_thread_category_mask);
    value |= (AllocateThreadId(name, allocate_flags) << 4) & g_thread_id_mask;
    value |= ((allocate_flags & ThreadId::AllocateFlags::DYNAMIC) ? 1u : 0u) << 31;

    return value;
}

const ThreadId& ThreadId::Current()
{
    return Threads::CurrentThreadId();
}

const ThreadId& ThreadId::Invalid()
{
    return invalid;
}

ThreadId::ThreadId(Name name, bool force_unique)
    : ThreadId(name, THREAD_CATEGORY_NONE, force_unique)
{
}

ThreadId::ThreadId(Name name, ThreadCategory category, bool force_unique)
    : ThreadId(
          name,
          category,
          AllocateFlags::DYNAMIC
              | (force_unique ? AllocateFlags::FORCE_UNIQUE : AllocateFlags::NONE))
{
}

ThreadId::ThreadId(Name name, ThreadCategory category, uint32 allocate_flags)
    : m_name(name),
      m_value(MakeThreadIdValue(name, category, allocate_flags))
{
}

#pragma endregion ThreadId

#pragma region StaticThreadId

StaticThreadId::StaticThreadId(Name name, bool force_unique)
    : ThreadId(name, THREAD_CATEGORY_NONE, force_unique ? AllocateFlags::FORCE_UNIQUE : AllocateFlags::NONE)
{
}

StaticThreadId::StaticThreadId(uint32 static_thread_index)
{
    m_name = GetStaticThreadIdCache().FindNameByIndex(static_thread_index + 1);
    m_value = ((1u << static_thread_index) << 4) & g_thread_id_mask;
}

#pragma endregion StaticThreadId

} // namespace threading
} // namespace hyperion
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/profiling/ProfileScope.hpp>
#include <util/profiling/PerformanceClock.hpp>

#include <core/containers/LinkedList.hpp>

#include <core/threading/Threads.hpp>

namespace hyperion {

// for debugging
thread_local uint32 profile_iteration = 0;

#pragma region ProfileScopeEntry

struct ProfileScopeEntry
{

    const ANSIStringView            label;
    const ANSIStringView            location;
    uint64                          start_timestamp_us;
    uint64                          measured_time_us;

    ProfileScopeEntry               *parent = nullptr;
    LinkedList<ProfileScopeEntry>   children;

    ProfileScopeEntry(ANSIStringView label, ANSIStringView location, ProfileScopeEntry *parent = nullptr)
        : label(label),
          location(location),
          start_timestamp_us(0),
          measured_time_us(0),
          parent(parent)
    {
        StartMeasure();
    }

    HYP_FORCE_INLINE
    void StartMeasure()
    {
        start_timestamp_us = PerformanceClock::Now();
        measured_time_us = 0;
    }

    HYP_FORCE_INLINE
    void SaveDiff()
    {
        measured_time_us = PerformanceClock::TimeSince(start_timestamp_us);
    }
};

#pragma endregion ProfileScopeEntry

#pragma region ProfileScopeStack

static void DebugLogProfileScopeEntry(ProfileScopeEntry *entry, int depth = 0)
{
    if (depth > 0) {
        for (int i = 0; i < depth; i++) {
            putchar(int(' '));
        }

        DebugLog(LogType::Debug, "Profile scope entry '%s': %llu us\n", entry->label.Data(), entry->measured_time_us);
    }

    for (ProfileScopeEntry &child : entry->children) {
        DebugLogProfileScopeEntry(&child, depth + 1);
    }
}

class ProfileScopeStack
{
public:
    ProfileScopeStack()
        : m_thread_id(Threads::CurrentThreadID()),
          m_root_entry("ROOT", ""),
          m_head(&m_root_entry)
    {
        m_root_entry.StartMeasure();
    }

    void Reset()
    {
        Threads::AssertOnThread(m_thread_id);

        m_root_entry.SaveDiff();
        
        if (profile_iteration++ == 25) {
            DebugLogProfileScopeEntry(&m_root_entry);
            profile_iteration = 0;
        }

        m_root_entry.children.Clear();
        m_root_entry.StartMeasure();

        m_head = &m_root_entry;
    }

    ProfileScopeEntry &Open(ANSIStringView label, ANSIStringView location)
    {
        Threads::AssertOnThread(m_thread_id);
        
        m_head = &m_head->children.EmplaceBack(label, location, m_head);
        return *m_head;
    }

    void Close()
    {
        Threads::AssertOnThread(m_thread_id);

        m_head->SaveDiff();

        AssertThrow(m_head->parent != nullptr);

        m_head = m_head->parent;
    }

private:
    ThreadID            m_thread_id;
    ProfileScopeEntry   m_root_entry;
    ProfileScopeEntry   *m_head;
};

thread_local ProfileScopeStack g_profile_scope_stack { };

#pragma endregion ProfileScopeStack

#pragma region ProfileScope

ProfileScope::ProfileScope(ANSIStringView label, ANSIStringView location)
    : entry(&g_profile_scope_stack.Open(label, location))
{
}

ProfileScope::~ProfileScope()
{
    g_profile_scope_stack.Close();
}

void ProfileScope::ResetForCurrentThread()
{
    g_profile_scope_stack.Reset();
}

#pragma endregion ProfileScope

} // namespace hyperion
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/profiling/ProfileScope.hpp>
#include <core/profiling/PerformanceClock.hpp>

#include <core/containers/LinkedList.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/memory/NotNullPtr.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/TaskThread.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/utilities/UUID.hpp>

#include <core/net/HTTPRequest.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/json/JSON.hpp>

namespace hyperion {
namespace profiling {

#pragma region ProfilerConnectionThread

class ProfilerConnectionThread final : public Thread<Scheduler, ProfilerConnection*>
{
public:
    ProfilerConnectionThread()
        : Thread(ThreadId(Name::Unique("ProfilerConnectionThread")), ThreadPriorityValue::LOWEST)
    {
    }

private:
    virtual void operator()(ProfilerConnection* profiler_connection) override
    {
        if (StartConnection(profiler_connection))
        {
            while (!m_stop_requested.Get(MemoryOrder::RELAXED))
            {
                DoWork(profiler_connection);
            }
        }
    }

    bool StartConnection(ProfilerConnection* profiler_connection);
    void DoWork(ProfilerConnection* profiler_connection);
};

#pragma endregion ProfilerConnectionThread

#pragma region ProfilerConnection

class ProfilerConnection
{
public:
    static ProfilerConnection& GetInstance()
    {
        static ProfilerConnection instance;

        return instance;
    }

    ProfilerConnection() = default;
    ProfilerConnection(const ProfilerConnection& other) = delete;
    ProfilerConnection& operator=(const ProfilerConnection& other) = delete;
    ProfilerConnection(ProfilerConnection&& other) noexcept = delete;
    ProfilerConnection& operator=(ProfilerConnection&& other) noexcept = delete;

    ~ProfilerConnection()
    {
        StopThread();

        for (Task<HTTPResponse>& task : m_requests)
        {
            task.Await();
        }
    }

    const ProfilerConnectionParams& GetParams() const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_params;
    }

    void SetParams(const ProfilerConnectionParams& params)
    {
        HYP_MT_CHECK_WRITE(m_data_race_detector);

        AssertThrowMsg(!m_thread.IsRunning(), "Cannot change profiler connection parameters while profiler connection thread is running");

        m_params = params;
    }

    void StartThread()
    {
        if (m_thread.IsRunning())
        {
            return;
        }

        m_thread.Start(this);
    }

    void StopThread()
    {
        if (m_thread.IsRunning())
        {
            m_thread.Stop();
        }

        if (m_thread.CanJoin())
        {
            m_thread.Join();
        }
    }

    void IterateRequests()
    {
        HYP_LOG(Profile, Info, "Iterate requests ({})", m_requests.Size());

        Threads::AssertOnThread(m_thread.GetID());

        // iterate over completed requests
        for (auto it = m_requests.Begin(); it != m_requests.End();)
        {
            if (it->IsCompleted())
            {
                it = m_requests.Erase(it);
                continue;
            }

            ++it;
        }
    }

    void Push(Array<json::JSONValue>&& values)
    {
        const ThreadId current_thread_id = Threads::CurrentThreadId();

        Array<json::JSONValue>* json_values_array = nullptr;

        { // critical section - may invalidate iterators
            Mutex::Guard guard(m_values_mutex);

            auto it = m_per_thread_values.Find(current_thread_id);

            if (it == m_per_thread_values.End())
            {
                it = m_per_thread_values.Insert(current_thread_id, MakeUnique<Array<json::JSONValue>>()).first;
            }

            json_values_array = it->second.Get();
        }

        // unique per thread; fine outside of mutex
        json_values_array->Concat(std::move(values));
    }

    bool StartConnection()
    {
        Threads::AssertOnThread(m_thread.GetID());

        if (m_params.endpoint_url.Empty())
        {
            HYP_LOG(Profile, Error, "Profiler connection endpoint URL not set, cannot start connection.");

            return false;
        }

        m_trace_id = UUID();

        json::JSONObject object;
        object["trace_id"] = m_trace_id.ToString();

        Task<HTTPResponse> start_request = HTTPRequest(m_params.endpoint_url + "/start", json::JSONValue(std::move(object)), HTTPMethod::POST)
                                               .Send();

        HYP_LOG(Profile, Info, "Waiting for profiler connection request to finish");

        HTTPResponse& response = start_request.Await();

        if (!response.IsSuccess())
        {
            HYP_LOG(Profile, Error, "Failed to connect to profiler connection endpoint! Status code: {}", response.GetStatusCode());

            return false;
        }

        return true;
    }

    void Submit()
    {
        Threads::AssertOnThread(m_thread.GetID());

        if (m_params.endpoint_url.Empty())
        {
            HYP_LOG(Profile, Warning, "Profiler connection endpoint URL not set, cannot submit results.");

            return;
        }

        HYP_LOG(Profile, Info, "Submitting profiler results to trace server...");

        json::JSONObject object;

        { // critical section
            Mutex::Guard guard(m_values_mutex);

            json::JSONArray groups_array;

            for (KeyValuePair<ThreadId, UniquePtr<json::JSONArray>>& it : m_per_thread_values)
            {
                json::JSONObject group_object;
                group_object["name"] = json::JSONString(it.first.GetName().LookupString());
                group_object["values"] = std::move(*it.second); // move it so it clears current values
                groups_array.PushBack(std::move(group_object));
            }

            object["groups"] = std::move(groups_array);
        }

        // Send request with all queued data
        HTTPRequest request(m_params.endpoint_url + "/results", json::JSONValue(std::move(object)), HTTPMethod::POST);
        m_requests.PushBack(request.Send());
    }

private:
    ProfilerConnectionParams m_params;

    UUID m_trace_id;
    ProfilerConnectionThread m_thread;

    FlatMap<ThreadId, UniquePtr<json::JSONArray>> m_per_thread_values;
    mutable Mutex m_values_mutex;

    Array<Task<HTTPResponse>> m_requests;

    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

HYP_API void StartProfilerConnectionThread(const ProfilerConnectionParams& params)
{
#ifdef HYP_ENABLE_PROFILE
    ProfilerConnection::GetInstance().SetParams(params);
    ProfilerConnection::GetInstance().StartThread();
#endif
}

HYP_API void StopProfilerConnectionThread()
{
#ifdef HYP_ENABLE_PROFILE
    ProfilerConnection::GetInstance().StopThread();
#endif
}

#pragma endregion ProfilerConnection

bool ProfilerConnectionThread::StartConnection(ProfilerConnection* profiler_connection)
{
    return profiler_connection->StartConnection();
}

void ProfilerConnectionThread::DoWork(ProfilerConnection* profiler_connection)
{
    profiler_connection->IterateRequests();

    Threads::Sleep(100);

    profiler_connection->Submit();
}

#pragma region ProfileScopeEntry

struct ProfileScopeEntry
{
    const ANSIString label;
    const ANSIStringView location;
    uint64 start_timestamp_us;
    uint64 measured_time_us;

    ProfileScopeEntry* parent = nullptr;
    LinkedList<ProfileScopeEntry> children;

    ProfileScopeEntry(ANSIStringView label, ANSIStringView location, ProfileScopeEntry* parent = nullptr)
        : label(label),
          location(location),
          start_timestamp_us(0),
          measured_time_us(0),
          parent(parent)
    {
        StartMeasure();
    }

    ProfileScopeEntry(const ProfileScopeEntry& other) = delete;
    ProfileScopeEntry& operator=(const ProfileScopeEntry& other) = delete;

    HYP_FORCE_INLINE void StartMeasure()
    {
        start_timestamp_us = PerformanceClock::Now();
        measured_time_us = 0;
    }

    HYP_FORCE_INLINE void SaveDiff()
    {
        measured_time_us = PerformanceClock::TimeSince(start_timestamp_us);
    }

    json::JSONValue ToJSON(ProfileScopeEntry* parent_scope = nullptr) const
    {
        json::JSONObject object;
        object["label"] = json::JSONString(label);
        object["location"] = json::JSONString(location);
        object["start_timestamp_ms"] = json::JSONNumber(start_timestamp_us / 1000);
        object["measured_time_us"] = json::JSONNumber(measured_time_us);

        json::JSONArray children_array;

        for (const ProfileScopeEntry& child : children)
        {
            children_array.PushBack(child.ToJSON());
        }

        object["children"] = std::move(children_array);

        return json::JSONValue(std::move(object));
    }
};

#pragma endregion ProfileScopeEntry

#pragma region ProfileScopeEntryQueue

struct ProfileScopeEntryQueue
{
    Time start_time;
    Array<ProfileScopeEntry> entries;

    json::JSONValue ToJSON() const
    {
        json::JSONArray array;

        for (const ProfileScopeEntry& entry : entries)
        {
            array.PushBack(entry.ToJSON());
        }

        json::JSONObject object;
        object["start_time"] = uint64(start_time);
        object["entries"] = std::move(array);

        return json::JSONValue(std::move(object));
    }
};

#pragma endregion ProfileScopeEntryQueue

#pragma region ProfileScopeStack

static void DebugLogProfileScopeEntry(ProfileScopeEntry* entry, int depth = 0)
{
    if (depth > 0)
    {
        for (int i = 0; i < depth; i++)
        {
            putchar(int(' '));
        }

        DebugLog(LogType::Debug, "Profile scope entry '%s': %llu us\n", entry->label.Data(), entry->measured_time_us);
    }

    for (ProfileScopeEntry& child : entry->children)
    {
        DebugLogProfileScopeEntry(&child, depth + 1);
    }
}

class ProfileScopeStack
{
public:
    ProfileScopeStack()
        : m_thread_id(Threads::CurrentThreadId()),
          m_root_entry("ROOT", ""),
          m_head(&m_root_entry)
    {
        m_root_entry.StartMeasure();
    }

    ~ProfileScopeStack() = default;

    void Reset()
    {
        Threads::AssertOnThread(m_thread_id);

        m_root_entry.SaveDiff();

        if (ProfilerConnection::GetInstance().GetParams().enabled)
        {
            m_queue.PushBack(m_root_entry.ToJSON());

            if (m_queue.Size() >= 100)
            {
                // DebugLogProfileScopeEntry(&m_root_entry);

                ProfilerConnection::GetInstance().Push(std::move(m_queue));
            }
        }

        m_root_entry.children.Clear();
        m_root_entry.StartMeasure();

        m_head = &m_root_entry;
    }

    ProfileScopeEntry& Open(ANSIStringView label, ANSIStringView location)
    {
        Threads::AssertOnThread(m_thread_id);

        m_head = &m_head->children.EmplaceBack(label, location, m_head);
        return *m_head;
    }

    void Close()
    {
        Threads::AssertOnThread(m_thread_id);

        m_head->SaveDiff();

        if (m_head != &m_root_entry)
        {
            // m_head should not be set to nullptr
            AssertThrow(m_head->parent != nullptr);
            m_head = m_head->parent;
        }
    }

private:
    ThreadId m_thread_id;
    ProfileScopeEntry m_root_entry;
    NotNullPtr<ProfileScopeEntry> m_head;
    json::JSONArray m_queue;
};

#pragma endregion ProfileScopeStack

#pragma region ProfileScope

ProfileScopeStack& ProfileScope::GetProfileScopeStackForCurrentThread()
{
    static thread_local ProfileScopeStack profile_scope_stack;

    return profile_scope_stack;
}

void ProfileScope::ResetForCurrentThread()
{
    GetProfileScopeStackForCurrentThread().Reset();
}

ProfileScope::ProfileScope(ANSIStringView label, ANSIStringView location)
    : entry(&GetProfileScopeStackForCurrentThread().Open(label, location))
{
}

ProfileScope::~ProfileScope()
{
    GetProfileScopeStackForCurrentThread().Close();
}

#pragma endregion ProfileScope

} // namespace profiling
} // namespace hyperion
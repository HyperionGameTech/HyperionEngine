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

#include <core/utilities/Uuid.hpp>

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
    virtual void operator()(ProfilerConnection* profilerConnection) override
    {
        if (StartConnection(profilerConnection))
        {
            while (!m_stopRequested.Get(MemoryOrder::RELAXED))
            {
                DoWork(profilerConnection);
            }
        }
    }

    bool StartConnection(ProfilerConnection* profilerConnection);
    void DoWork(ProfilerConnection* profilerConnection);
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
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_params;
    }

    void SetParams(const ProfilerConnectionParams& params)
    {
        HYP_MT_CHECK_WRITE(m_dataRaceDetector);

        HYP_CORE_ASSERT(!m_thread.IsRunning(), "Cannot change profiler connection parameters while profiler connection thread is running");

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

        AssertOnThread(m_thread.Id());

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
        const ThreadId currentThreadId = CurrentThreadId();

        json::JSONArray* jsonValuesArray = nullptr;

        { // critical section - may invalidate iterators
            Mutex::Guard guard(m_valuesMutex);

            auto it = m_perThreadValues.Find(currentThreadId);

            if (it == m_perThreadValues.End())
            {
                it = m_perThreadValues.Insert(currentThreadId, MakeUnique<json::JSONArray>()).first;
            }

            jsonValuesArray = it->second.Get();
        }

        // unique per thread; fine outside of mutex
        jsonValuesArray->Concat(values);
    }

    bool StartConnection()
    {
        AssertOnThread(m_thread.Id());

        if (m_params.endpointUrl.Empty())
        {
            HYP_LOG(Profile, Error, "Profiler connection endpoint URL not set, cannot start connection.");

            return false;
        }

        m_traceId = Uuid();

        json::JSONObject object;
        object["trace_id"] = m_traceId.ToString();

        Task<HTTPResponse> startRequest = HTTPRequest(m_params.endpointUrl + "/start", json::JSONValue(std::move(object)), HTTPMethod::POST)
                                              .Send();

        HYP_LOG(Profile, Info, "Waiting for profiler connection request to finish");

        HTTPResponse& response = startRequest.Await();

        if (!response.IsSuccess())
        {
            HYP_LOG(Profile, Error, "Failed to connect to profiler connection endpoint! Status code: {}", response.GetStatusCode());

            return false;
        }

        return true;
    }

    void Submit()
    {
        AssertOnThread(m_thread.Id());

        if (m_params.endpointUrl.Empty())
        {
            HYP_LOG(Profile, Warning, "Profiler connection endpoint URL not set, cannot submit results.");

            return;
        }

        HYP_LOG(Profile, Info, "Submitting profiler results to trace server...");

        json::JSONObject object;

        { // critical section
            Mutex::Guard guard(m_valuesMutex);

            json::JSONArray groupsArray;

            for (KeyValuePair<ThreadId, UniquePtr<json::JSONArray>>& it : m_perThreadValues)
            {
                json::JSONObject groupObject;
                groupObject["name"] = json::JSONString(it.first.GetName().LookupString());
                groupObject["values"] = std::move(*it.second); // move it so it clears current values
                groupsArray.PushBack(std::move(groupObject));
            }

            object["groups"] = std::move(groupsArray);
        }

        // Send request with all queued data
        HTTPRequest request(m_params.endpointUrl + "/results", json::JSONValue(std::move(object)), HTTPMethod::POST);
        m_requests.PushBack(request.Send());
    }

private:
    ProfilerConnectionParams m_params;

    Uuid m_traceId;
    ProfilerConnectionThread m_thread;

    FlatMap<ThreadId, UniquePtr<json::JSONArray>> m_perThreadValues;
    mutable Mutex m_valuesMutex;

    Array<Task<HTTPResponse>> m_requests;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
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

bool ProfilerConnectionThread::StartConnection(ProfilerConnection* profilerConnection)
{
    return profilerConnection->StartConnection();
}

void ProfilerConnectionThread::DoWork(ProfilerConnection* profilerConnection)
{
    profilerConnection->IterateRequests();

    ThreadSleep(100);

    profilerConnection->Submit();
}

#pragma region ProfileScopeEntry

struct ProfileScopeEntry
{
    const ANSIString label;
    const ANSIStringView location;
    uint64 startTimestampUs;
    uint64 measuredTimeUs;

    ProfileScopeEntry* parent = nullptr;
    LinkedList<ProfileScopeEntry> children;

    ProfileScopeEntry(ANSIStringView label, ANSIStringView location, ProfileScopeEntry* parent = nullptr)
        : label(label),
          location(location),
          startTimestampUs(0),
          measuredTimeUs(0),
          parent(parent)
    {
        StartMeasure();
    }

    ProfileScopeEntry(const ProfileScopeEntry& other) = delete;
    ProfileScopeEntry& operator=(const ProfileScopeEntry& other) = delete;

    HYP_FORCE_INLINE void StartMeasure()
    {
        startTimestampUs = PerformanceClock::Now();
        measuredTimeUs = 0;
    }

    HYP_FORCE_INLINE void SaveDiff()
    {
        measuredTimeUs = PerformanceClock::TimeSince(startTimestampUs);
    }

    json::JSONValue ToJSON(ProfileScopeEntry* parentScope = nullptr) const
    {
        json::JSONObject object;
        object["label"] = json::JSONString(label);
        object["location"] = json::JSONString(location);
        object["start_timestamp_ms"] = json::JSONNumber(startTimestampUs / 1000);
        object["measured_time_us"] = json::JSONNumber(measuredTimeUs);

        json::JSONArray childrenArray;

        for (const ProfileScopeEntry& child : children)
        {
            childrenArray.PushBack(child.ToJSON());
        }

        object["children"] = std::move(childrenArray);

        return json::JSONValue(std::move(object));
    }
};

#pragma endregion ProfileScopeEntry

#pragma region ProfileScopeEntryQueue

struct ProfileScopeEntryQueue
{
    Time startTime;
    Array<ProfileScopeEntry> entries;

    json::JSONValue ToJSON() const
    {
        json::JSONArray array;

        for (const ProfileScopeEntry& entry : entries)
        {
            array.PushBack(entry.ToJSON());
        }

        json::JSONObject object;
        object["start_time"] = uint64(startTime);
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

        HYP_LOG(Profile, Debug, "Profile scope entry '{}': {} us\n", entry->label, entry->measuredTimeUs);
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
        : m_threadId(CurrentThreadId()),
          m_rootEntry("ROOT", ""),
          m_head(&m_rootEntry)
    {
        m_rootEntry.StartMeasure();
    }

    ~ProfileScopeStack() = default;

    void Reset()
    {
        AssertOnThread(m_threadId);

        m_rootEntry.SaveDiff();

        if (ProfilerConnection::GetInstance().GetParams().enabled)
        {
            m_queue.PushBack(m_rootEntry.ToJSON());

            if (m_queue.Size() >= 100)
            {
                // DebugLogProfileScopeEntry(&m_rootEntry);

                ProfilerConnection::GetInstance().Push(std::move(m_queue));
            }
        }

        m_rootEntry.children.Clear();
        m_rootEntry.StartMeasure();

        m_head = &m_rootEntry;
    }

    ProfileScopeEntry& Open(ANSIStringView label, ANSIStringView location)
    {
        AssertOnThread(m_threadId);

        m_head = &m_head->children.EmplaceBack(label, location, m_head);
        return *m_head;
    }

    void Close()
    {
        AssertOnThread(m_threadId);

        m_head->SaveDiff();

        if (m_head != &m_rootEntry)
        {
            // m_head should not be set to nullptr
            HYP_CORE_ASSERT(m_head->parent != nullptr);
            m_head = m_head->parent;
        }
    }

private:
    ThreadId m_threadId;
    ProfileScopeEntry m_rootEntry;
    NotNullPtr<ProfileScopeEntry> m_head;
    json::JSONArray m_queue;
};

#pragma endregion ProfileScopeStack

#pragma region ProfileScope

ProfileScopeStack& ProfileScope::GetProfileScopeStackForCurrentThread()
{
    static thread_local ProfileScopeStack profileScopeStack;

    return profileScopeStack;
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
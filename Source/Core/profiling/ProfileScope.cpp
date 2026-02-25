/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/profiling/ProfileScope.hpp>
#include <Core/profiling/PerformanceClock.hpp>

#include <Core/containers/LinkedList.hpp>
#include <Core/containers/FlatMap.hpp>

#include <Core/memory/NotNullPtr.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/Thread.hpp>
#include <Core/threading/TaskThread.hpp>
#include <Core/threading/DataRaceDetector.hpp>

#include <Core/utilities/Uuid.hpp>

#include <Core/net/HTTPRequest.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <Core/cli/CommandLine.hpp>

#include <Core/json/JSON.hpp>

namespace Hyperion {
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
            while (!m_stopRequested.Load())
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
        HYP_LOG(Profile, Verbose, "Iterate requests ({})", m_requests.Size());

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

    void Push(Array<JSON::Value>&& values)
    {
        const ThreadId currentThreadId = CurrentThreadId();

        JSON::JArray* jsonValuesArray = nullptr;

        { // critical section - may invalidate iterators
            Mutex::Guard guard(m_valuesMutex);

            auto it = m_perThreadValues.Find(currentThreadId);

            if (it == m_perThreadValues.End())
            {
                it = m_perThreadValues.Insert(currentThreadId, MakeUnique<JSON::JArray>()).first;
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

        m_traceId = UUID();

        JSON::Object object;
        object["trace_id"] = m_traceId.ToString();

        Task<HTTPResponse> startRequest = HTTPRequest(m_params.endpointUrl + "/start", JSON::Value(std::move(object)), HTTPMethod::POST)
                                              .Send();

        HYP_LOG(Profile, Verbose, "Waiting for profiler connection request to finish");

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

        HYP_LOG(Profile, Verbose, "Submitting profiler results to trace server...");

        JSON::Object object;

        { // critical section
            Mutex::Guard guard(m_valuesMutex);

            JSON::JArray groupsArray;

            for (KeyValuePair<ThreadId, UniquePtr<JSON::JArray>>& it : m_perThreadValues)
            {
                JSON::Object groupObject;
                groupObject["name"] = JSON::JString(it.first.GetName().LookupString());
                groupObject["values"] = std::move(*it.second); // move it so it clears current values
                groupsArray.PushBack(std::move(groupObject));
            }

            object["groups"] = std::move(groupsArray);
        }

        // Send request with all queued data
        HTTPRequest request(m_params.endpointUrl + "/results", JSON::Value(std::move(object)), HTTPMethod::POST);
        m_requests.PushBack(request.Send());
    }

private:
    ProfilerConnectionParams m_params;

    UUID m_traceId;
    ProfilerConnectionThread m_thread;

    FlatMap<ThreadId, UniquePtr<JSON::JArray>> m_perThreadValues;
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
    uint64 startTimestamp;
    double measuredTimeMs;

    ProfileScopeEntry* parent = nullptr;
    LinkedList<ProfileScopeEntry> children;

    ProfileScopeEntry(ANSIStringView label, ANSIStringView location, ProfileScopeEntry* parent = nullptr)
        : label(label),
          location(location),
          startTimestamp(0),
          measuredTimeMs(0),
          parent(parent)
    {
        StartMeasure();
    }

    ProfileScopeEntry(const ProfileScopeEntry& other) = delete;
    ProfileScopeEntry& operator=(const ProfileScopeEntry& other) = delete;

    HYP_FORCE_INLINE void StartMeasure()
    {
        startTimestamp = PerformanceClock::Now();
        measuredTimeMs = 0;
    }

    HYP_FORCE_INLINE void SaveDiff()
    {
        measuredTimeMs = PerformanceClock::TimeSince(startTimestamp);
    }

    JSON::Value ToJSON(ProfileScopeEntry* parentScope = nullptr) const
    {
        JSON::Object object;
        object["label"] = JSON::JString(label);
        object["location"] = JSON::JString(location);
        object["start_timestamp_ms"] = JSON::Number(PerformanceClock::ToMilliseconds(startTimestamp));
        object["measured_time_ms"] = JSON::Number(measuredTimeMs);

        JSON::JArray childrenArray;

        for (const ProfileScopeEntry& child : children)
        {
            childrenArray.PushBack(child.ToJSON());
        }

        object["children"] = std::move(childrenArray);

        return JSON::Value(std::move(object));
    }
};

#pragma endregion ProfileScopeEntry

#pragma region ProfileScopeEntryQueue

struct ProfileScopeEntryQueue
{
    Time startTime;
    Array<ProfileScopeEntry> entries;

    JSON::Value ToJSON() const
    {
        JSON::JArray array;

        for (const ProfileScopeEntry& entry : entries)
        {
            array.PushBack(entry.ToJSON());
        }

        JSON::Object object;
        object["start_time"] = uint64(startTime);
        object["entries"] = std::move(array);

        return JSON::Value(std::move(object));
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

        HYP_LOG(Profile, Debug, "Profile scope entry '{}': {} ms\n", entry->label, entry->measuredTimeMs);
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
    JSON::JArray m_queue;
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
} // namespace Hyperion

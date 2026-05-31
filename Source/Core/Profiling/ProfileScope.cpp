/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Profiling/ProfileScope.hpp>
#include <Core/Profiling/PerformanceClock.hpp>

#include <Core/Containers/LinkedList.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Utilities/Pair.hpp>

#include <Core/Memory/NotNullPtr.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>
#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Thread.hpp>
#include <Core/Threading/TaskThread.hpp>
#include <Core/Threading/ThreadLocalStorage.hpp>
#include <Core/Threading/DataRaceDetector.hpp>

#include <Core/Utilities/Uuid.hpp>

#include <Core/Net/HTTPRequest.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/JSON/JSON.hpp>

#include <Core/Core.hpp>

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

CORE_API void StartProfilerConnectionThread(const ProfilerConnectionParams& params)
{
#if HYP_ENABLE_PROFILE
    if (CoreApi::IsProfilingEnabled())
    {
        ProfilerConnection::GetInstance().SetParams(params);
        ProfilerConnection::GetInstance().StartThread();
    }
#endif
}

CORE_API void StopProfilerConnectionThread()
{
#if HYP_ENABLE_PROFILE
    if (CoreApi::IsProfilingEnabled())
    {
        ProfilerConnection::GetInstance().StopThread();
    }
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

// If we reach this number, the thread probably isn't resetting the scopes in a consistent way and we need to reset at the threshold
// for some threads this makes sense, others that have a defined loop like render, sim etc. Will be reset at the top of the frame
static constexpr uint32 MaxRecordedScopes = 4096;

static constexpr uint32 MaxHotFunctions = 10;

static void DebugLogProfileScopeEntry(ProfileScopeEntry* entry, int depth = 0)
{
    if (depth > 0)
    {
        for (int i = 0; i < depth; i++)
        {
            putchar(int(' '));
        }

        HYP_LOG(Profile, Verbose, "Profile scope entry '{}': {} ms\n", entry->label, entry->measuredTimeMs);
    }

    for (ProfileScopeEntry& child : entry->children)
    {
        DebugLogProfileScopeEntry(&child, depth + 1);
    }
}

class ProfileScopeStack
{
    using TimeByFunctionMap = TMap<ANSIString, double, ThreadAllocator>;

public:
    ProfileScopeStack()
        : m_threadId(CurrentThreadId()),
          m_rootEntry("ROOT", ""),
          m_head(&m_rootEntry),
          m_numRecordedScopes(0)
    {
        m_rootEntry.StartMeasure();
    }

    ~ProfileScopeStack() = default;

    void Reset()
    {
        AssertOnThread(m_threadId);

        m_rootEntry.SaveDiff();

        BuildHotFunctions();

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

        m_numRecordedScopes = 0;
    }

    ProfileScopeEntry& Open(ANSIStringView label, ANSIStringView location)
    {
        AssertOnThread(m_threadId);

        if (m_numRecordedScopes >= MaxRecordedScopes
            && m_head == &m_rootEntry) // only reset if at root (don't mess up nesting)
        {
            Reset();
        }

        ++m_numRecordedScopes;

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

    HYP_FORCE_INLINE const Time& GetLastHotFunctionsUpdateTimestamp() const
    {
        TSharedLock lock(m_hotFunctionsMutex);
        return m_lastHotFunctionsUpdateTimestamp;
    }

    void CollectHotFunctions(Array<Pair<ANSIString, double>>& outHotFunctions)
    {
        TSharedLock lock(m_hotFunctionsMutex);

        outHotFunctions.Reserve(outHotFunctions.Size() + m_numHotFunctions);

        const ANSIString threadIdString = *m_threadId.GetName();

        for (uint32 i = 0; i < m_numHotFunctions; ++i)
        {
            outHotFunctions.EmplaceBack(threadIdString + ":" + m_hotFunctions[i].first, m_hotFunctions[i].second);
        }
    }

private:
    void BuildHotFunctions()
    {
        TUniqueLock lock(m_hotFunctionsMutex);

        TimeByFunctionMap timeByFunction;
        CollectTimeByFunction_Internal(&m_rootEntry, timeByFunction);

        m_numHotFunctions = 0;

        CollectHotFunctions_Internal(timeByFunction);

        m_lastHotFunctionsUpdateTimestamp = Time::Now();
    }

    void CollectTimeByFunction_Internal(ProfileScopeEntry* entry, TimeByFunctionMap& totalTimeByFunction)
    {
        // assume mutex is locked

        if (entry != &m_rootEntry)
        {
            totalTimeByFunction[entry->label.Any() ? ANSIStringView(entry->label) : entry->location] += entry->measuredTimeMs;
        }

        for (ProfileScopeEntry& child : entry->children)
        {
            CollectTimeByFunction_Internal(&child, totalTimeByFunction);
        }
    }

    void CollectHotFunctions_Internal(const TimeByFunctionMap& timeByFunction)
    {
        // assume mutex is locked

        for (const KeyValuePair<ANSIString, double>& it : timeByFunction)
        {
            uint32 insertPos = m_numHotFunctions;

            for (uint32 i = 0; i < m_numHotFunctions; ++i)
            {
                if (it.second > m_hotFunctions[i].second)
                {
                    insertPos = i;
                    break;
                }
            }

            if (insertPos < MaxHotFunctions)
            {
                const uint32 shiftEnd = m_numHotFunctions < MaxHotFunctions ? m_numHotFunctions : MaxHotFunctions - 1;

                for (uint32 i = shiftEnd; i > insertPos; --i)
                {
                    m_hotFunctions[i] = std::move(m_hotFunctions[i - 1]);
                }

                m_hotFunctions[insertPos] = { it.first, it.second };

                if (m_numHotFunctions < MaxHotFunctions)
                {
                    ++m_numHotFunctions;
                }
            }
        }
    }

    ThreadId m_threadId;
    ProfileScopeEntry m_rootEntry;
    NotNullPtr<ProfileScopeEntry> m_head;
    JSON::JArray m_queue;
    uint32 m_numRecordedScopes;

    SharedMutex m_hotFunctionsMutex; // since we will be ingesting the data from another thread for reading.
    FixedArray<Pair<ANSIString, double>, MaxHotFunctions> m_hotFunctions;
    uint32 m_numHotFunctions = 0;
    Time m_lastHotFunctionsUpdateTimestamp;
};

#pragma endregion ProfileScopeStack

#pragma region ProfileScope

static constexpr uint32 MaxRegisteredProfileScopeStacks = 64;
static ProfileScopeStack* s_allRegisteredProfileScopeStacks[MaxRegisteredProfileScopeStacks];
static volatile int64 s_numRegisteredProfileScopeStacks;
static volatile int64 s_profileScopeStacksBitMask;

static thread_local ProfileScopeStack* s_profileScopeStack;
static thread_local uint32 s_profileScopeStackIndex;

void CollectAllHotFunctions(Array<Pair<ANSIString, double>>& outHotFunctions)
{
    const Time now = Time::Now();

    const uint64 mask = std::bit_cast<uint64>(AtomicAdd(&s_profileScopeStacksBitMask, 0));

    FOR_EACH_BIT(mask, iter)
    {
        ProfileScopeStack* profileScopeStack = s_allRegisteredProfileScopeStacks[iter];

        if (now - profileScopeStack->GetLastHotFunctionsUpdateTimestamp() >= TimeDiff(3000))
        {
            // skip if time diff >= 3s.
            // if it hasn't been updated in a while it's not really relevant anymore
            continue;
        }

        profileScopeStack->CollectHotFunctions(outHotFunctions);
    }

    std::sort(outHotFunctions.Begin(), outHotFunctions.End(), [](const Pair<ANSIString, double>& a, const Pair<ANSIString, double>& b)
        {
            return a.second > b.second;
        });
}

ProfileScopeStack& ProfileScope::GetProfileScopeStackForCurrentThread()
{
    if (HYP_UNLIKELY(!s_profileScopeStack))
    {
        s_profileScopeStackIndex = uint32(AtomicAdd(&s_numRegisteredProfileScopeStacks, 1));
        Assert(s_profileScopeStackIndex < MaxRegisteredProfileScopeStacks, "Too many profile scope stacks registered");

        ThreadBase* currThread = CurrentThreadObject();
        if (currThread != nullptr)
        {
            // use thread local allocator

            s_profileScopeStack = (ProfileScopeStack*)GetDefaultAllocatorInstance<ThreadAllocator>()->Allocate(sizeof(ProfileScopeStack), alignof(ProfileScopeStack));
            AssertDebug(s_profileScopeStack != nullptr);

            new (s_profileScopeStack) ProfileScopeStack;

            s_allRegisteredProfileScopeStacks[s_profileScopeStackIndex] = s_profileScopeStack;

            AtomicBitOr(&s_profileScopeStacksBitMask, int64(1 << s_profileScopeStackIndex));

            currThread->AddOnExitCallback([]()
                {
                    AtomicBitAnd(&s_profileScopeStacksBitMask, ~(1 << s_profileScopeStackIndex));

                    s_allRegisteredProfileScopeStacks[s_profileScopeStackIndex] = nullptr;

                    s_profileScopeStack->~ProfileScopeStack();
                    GetDefaultAllocatorInstance<ThreadAllocator>()->Free(s_profileScopeStack);
                    s_profileScopeStack = nullptr;
                });
        }
        else
        {
            // fallback to use thread_local instance.
            // thread_local non-trivially constructible types have a performance overhead
            // (I have a reference to more info on this hanging around somewhere...)
            // but currently some threads don't have an object assoc'd so i'm adding this for now
            // as a temporary measure :)

            static thread_local ProfileScopeStack s_profileScopeStackInstance;
            s_profileScopeStack = &s_profileScopeStackInstance;

            AtomicBitOr(&s_profileScopeStacksBitMask, int64(1 << s_profileScopeStackIndex));

            s_allRegisteredProfileScopeStacks[s_profileScopeStackIndex] = s_profileScopeStack;
        }
    }

    return *s_profileScopeStack;
}

void ProfileScope::ResetForCurrentThread()
{
    GetProfileScopeStackForCurrentThread().Reset();
}

ProfileScope::ProfileScope(ANSIStringView label, ANSIStringView location)
    : entry(CoreApi::IsProfilingEnabled() ? &GetProfileScopeStackForCurrentThread().Open(label, location) : nullptr)
{
}

ProfileScope::~ProfileScope()
{
    if (CoreApi::IsProfilingEnabled())
    {
        GetProfileScopeStackForCurrentThread().Close();
    }
}

#pragma endregion ProfileScope

} // namespace profiling
} // namespace Hyperion

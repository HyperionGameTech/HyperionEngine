/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/threading/util/ThreadId.hpp>
#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/utilities/EnumFlags.hpp>
#include <Core/utilities/Pair.hpp>
#include <Core/utilities/StringView.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/Array.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

enum class DataAccessFlags : uint32
{
    ACCESS_NONE = 0x0,
    ACCESS_READ = 0x1,
    ACCESS_WRITE = 0x2,
    ACCESS_RW = ACCESS_READ | ACCESS_WRITE
};

HYP_MAKE_ENUM_FLAGS(DataAccessFlags)

namespace threading {

#ifdef HYP_ENABLE_MT_CHECK

struct SuppressDataRaceDetectorContext
{
};

class CORE_API DataRaceDetector
{
public:
    static constexpr size_t numPreallocatedStates = g_maxStaticThreadIds;

    struct DataAccessState
    {
        ANSIStringView currentFunction;
        ANSIStringView message;
    };

    struct ThreadAccessState
    {
        ThreadId threadId;
        EnumFlags<DataAccessFlags> access = DataAccessFlags::ACCESS_NONE;
        uint32 originalIndex = ~0u;
        DataAccessState state;
    };

    class DataAccessScope
    {
    public:
        DataAccessScope(EnumFlags<DataAccessFlags> flags, const DataRaceDetector& detector, const DataAccessState& state = {});
        ~DataAccessScope();

    private:
        EnumFlags<DataAccessFlags> m_flags;
        DataRaceDetector& m_detector;
        ThreadId m_threadId;
    };

    DataRaceDetector();

    DataRaceDetector(const DataRaceDetector& other)
        : m_preallocatedStates(other.m_preallocatedStates),
          m_dynamicStates(other.m_dynamicStates),
          m_writers(other.m_writers.Get(MemoryOrder::ACQUIRE)),
          m_readers(other.m_readers.Get(MemoryOrder::ACQUIRE))
    {
    }

    DataRaceDetector& operator=(const DataRaceDetector& other) = delete;

    DataRaceDetector(DataRaceDetector&& other) noexcept
        : m_preallocatedStates(std::move(other.m_preallocatedStates)),
          m_dynamicStates(std::move(other.m_dynamicStates)),
          m_writers(other.m_writers.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
          m_readers(other.m_readers.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
    {
    }

    DataRaceDetector& operator=(DataRaceDetector&& other) = delete;
    ~DataRaceDetector();

    EnumFlags<DataAccessFlags> AddAccess(ThreadId threadId, EnumFlags<DataAccessFlags> accessFlags, const DataAccessState& state = {});
    void RemoveAccess(ThreadId threadId, EnumFlags<DataAccessFlags> accessFlags);

private:
    void LogDataRace(uint64 readersMask, uint64 writersMask) const;
    void GetThreadIds(uint64 readersMask, uint64 writersMask, Array<Pair<ThreadId, DataAccessState>>& outReaderThreadIds, Array<Pair<ThreadId, DataAccessState>>& outWriterThreadIds) const;

    Array<ThreadAccessState, DynamicAllocator> m_preallocatedStates;

    Array<ThreadAccessState, DynamicAllocator> m_dynamicStates;
    mutable Mutex m_dynamicStatesMutex;

    // Indices of ThreadAccessState
    AtomicVar<uint64> m_writers;
    AtomicVar<uint64> m_readers;
};
#else

class DataRaceDetector
{
public:
    class DataAccessScope
    {
    };
};

#endif

#ifdef HYP_ENABLE_MT_CHECK
#define HYP_DECLARE_MT_CHECK(_data_race_detector) DataRaceDetector _data_race_detector
#else
#define HYP_DECLARE_MT_CHECK(_data_race_detector)
#endif

} // namespace threading

using threading::DataRaceDetector;

#ifdef HYP_ENABLE_MT_CHECK
#define HYP_MT_CHECK_READ(_data_race_detector, ...) DataRaceDetector::DataAccessScope HYP_UNIQUE_NAME(_data_access_scope)(DataAccessFlags::ACCESS_READ, (_data_race_detector), { HYP_FUNCTION_NAME_LIT, ##__VA_ARGS__ })
#define HYP_MT_CHECK_WRITE(_data_race_detector, ...) DataRaceDetector::DataAccessScope HYP_UNIQUE_NAME(_data_access_scope)(DataAccessFlags::ACCESS_WRITE, (_data_race_detector), { HYP_FUNCTION_NAME_LIT, ##__VA_ARGS__ })
#define HYP_MT_CHECK_RW(_data_race_detector, ...) DataRaceDetector::DataAccessScope HYP_UNIQUE_NAME(_data_access_scope)(DataAccessFlags::ACCESS_RW, (_data_race_detector), { HYP_FUNCTION_NAME_LIT, ##__VA_ARGS__ })
#else
#define HYP_MT_CHECK_READ(_data_race_detector, ...)
#define HYP_MT_CHECK_WRITE(_data_race_detector, ...)
#define HYP_MT_CHECK_RW(_data_race_detector, ...)
#endif

} // namespace Hyperion

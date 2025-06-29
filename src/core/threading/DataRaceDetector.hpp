/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DATA_RACE_DETECTOR_HPP
#define HYPERION_DATA_RACE_DETECTOR_HPP

#include <core/Defines.hpp>

#include <core/threading/ThreadId.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/StringView.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/Array.hpp>

#include <Types.hpp>

namespace hyperion {

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

class HYP_API DataRaceDetector
{
public:
    static constexpr SizeType num_preallocated_states = g_max_static_thread_ids;

    struct DataAccessState
    {
        ANSIStringView current_function;
        ANSIStringView message;
    };

    struct ThreadAccessState
    {
        ThreadId thread_id;
        EnumFlags<DataAccessFlags> access = DataAccessFlags::ACCESS_NONE;
        uint32 original_index = ~0u;
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
        ThreadId m_thread_id;
    };

    DataRaceDetector();

    DataRaceDetector(const DataRaceDetector& other)
        : m_preallocated_states(other.m_preallocated_states),
          m_dynamic_states(other.m_dynamic_states),
          m_writers(other.m_writers.Get(MemoryOrder::ACQUIRE)),
          m_readers(other.m_readers.Get(MemoryOrder::ACQUIRE))
    {
    }

    DataRaceDetector& operator=(const DataRaceDetector& other) = delete;

    DataRaceDetector(DataRaceDetector&& other) noexcept
        : m_preallocated_states(std::move(other.m_preallocated_states)),
          m_dynamic_states(std::move(other.m_dynamic_states)),
          m_writers(other.m_writers.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
          m_readers(other.m_readers.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
    {
    }

    DataRaceDetector& operator=(DataRaceDetector&& other) = delete;
    ~DataRaceDetector();

    EnumFlags<DataAccessFlags> AddAccess(ThreadId thread_id, EnumFlags<DataAccessFlags> access_flags, const DataAccessState& state = {});
    void RemoveAccess(ThreadId thread_id, EnumFlags<DataAccessFlags> access_flags);

private:
    void LogDataRace(uint64 readers_mask, uint64 writers_mask) const;
    void GetThreadIds(uint64 readers_mask, uint64 writers_mask, Array<Pair<ThreadId, DataAccessState>>& out_reader_thread_ids, Array<Pair<ThreadId, DataAccessState>>& out_writer_thread_ids) const;

    Array<ThreadAccessState, DynamicAllocator> m_preallocated_states;

    Array<ThreadAccessState, DynamicAllocator> m_dynamic_states;
    mutable Mutex m_dynamic_states_mutex;

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

} // namespace hyperion

#endif
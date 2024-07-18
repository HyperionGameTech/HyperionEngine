/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DATA_RACE_DETECTOR_HPP
#define HYPERION_DATA_RACE_DETECTOR_HPP

#include <core/Defines.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Mutex.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/StringView.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/Array.hpp>

#include <math/MathUtil.hpp>

#include <Types.hpp>

#ifdef HYP_DEBUG_MODE
    #define HYP_ENABLE_MT_CHECK
#endif

namespace hyperion {

enum class DataAccessFlags : uint32
{
    ACCESS_NONE     = 0x0,
    ACCESS_READ     = 0x1,
    ACCESS_WRITE    = 0x2,
    ACCESS_RW       = ACCESS_READ | ACCESS_WRITE
};

HYP_MAKE_ENUM_FLAGS(DataAccessFlags)

namespace threading {

#ifdef HYP_ENABLE_MT_CHECK
class HYP_API DataRaceDetector
{
    struct ThreadAccessState;
    
    static constexpr SizeType num_preallocated_states = MathUtil::FastLog2_Pow2(uint64(ThreadName::THREAD_STATIC));

    static const FixedArray<ThreadAccessState, num_preallocated_states> &GetPreallocatedStates();

public:
    class DataAccessScope
    {
    public:
        DataAccessScope(EnumFlags<DataAccessFlags> flags, const DataRaceDetector &detector)
            : m_detector(const_cast<DataRaceDetector &>(detector)),
              m_thread_id(ThreadID::Current())
        {
            m_flags = m_detector.AddAccess(m_thread_id, flags);
        }

        DataAccessScope(EnumFlags<DataAccessFlags> flags, const DataRaceDetector &detector, const ANSIStringView &current_function)
            : m_detector(const_cast<DataRaceDetector &>(detector)),
              m_thread_id(ThreadID::Current())
        {
            m_flags = m_detector.AddAccess(m_thread_id, flags, current_function);
        }

        ~DataAccessScope()
        {
            m_detector.RemoveAccess(m_thread_id, m_flags);
        }

    private:
        EnumFlags<DataAccessFlags>  m_flags;
        DataRaceDetector            &m_detector;
        ThreadID                    m_thread_id;
    };

    DataRaceDetector();
    DataRaceDetector(const DataRaceDetector &other)             = delete;
    DataRaceDetector &operator=(const DataRaceDetector &other)  = delete;
    DataRaceDetector(DataRaceDetector &&other)                  = delete;
    DataRaceDetector &operator=(DataRaceDetector &&other)       = delete;
    ~DataRaceDetector();

    EnumFlags<DataAccessFlags> AddAccess(ThreadID thread_id, EnumFlags<DataAccessFlags> access_flags, const ANSIStringView &current_function = "<null>");
    void RemoveAccess(ThreadID thread_id, EnumFlags<DataAccessFlags> access_flags);

private:
    void LogDataRace(uint64 readers_mask, uint64 writers_mask) const;
    void GetThreadIDs(uint64 readers_mask, uint64 writers_mask, Array<Pair<ThreadID, ANSIStringView>> &out_reader_thread_ids, Array<Pair<ThreadID, ANSIStringView>> &out_writer_thread_ids) const;

    struct ThreadAccessState
    {
        ThreadID                    thread_id = ThreadID::Invalid();
        EnumFlags<DataAccessFlags>  access = DataAccessFlags::ACCESS_NONE;
        ANSIStringView              current_function;
    };

    FixedArray<ThreadAccessState, num_preallocated_states>  m_preallocated_states;

    Array<ThreadAccessState>                                m_dynamic_states;
    mutable Mutex                                           m_dynamic_states_mutex;

    // Indices of ThreadAccessState
    AtomicVar<uint64>                                       m_writers;
    AtomicVar<uint64>                                       m_readers;
};
#else

class DataRaceDetector { };

#endif

} // namespace threading

using threading::DataRaceDetector;

#ifdef HYP_ENABLE_MT_CHECK
    #define HYP_MT_CHECK_READ(_data_race_detector) DataRaceDetector::DataAccessScope HYP_UNIQUE_NAME(_data_access_scope)(DataAccessFlags::ACCESS_READ, (_data_race_detector), HYP_FUNCTION_NAME_LIT)
    #define HYP_MT_CHECK_WRITE(_data_race_detector) DataRaceDetector::DataAccessScope HYP_UNIQUE_NAME(_data_access_scope)(DataAccessFlags::ACCESS_WRITE, (_data_race_detector), HYP_FUNCTION_NAME_LIT)
    #define HYP_MT_CHECK_RW(_data_race_detector) DataRaceDetector::DataAccessScope HYP_UNIQUE_NAME(_data_access_scope)(DataAccessFlags::ACCESS_RW, (_data_race_detector), HYP_FUNCTION_NAME_LIT)
#else
    #define HYP_MT_CHECK_READ(_data_race_detector)
    #define HYP_MT_CHECK_WRITE(_data_race_detector)
    #define HYP_MT_CHECK_RW(_data_race_detector)
#endif

} // namespace hyperion

#endif
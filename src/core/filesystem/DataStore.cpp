/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/filesystem/DataStore.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/utilities/Time.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <asset/Assets.hpp>

#include <HyperionEngine.hpp>

namespace hyperion {
namespace filesystem {

HYP_DEFINE_LOG_SUBCHANNEL(DataStore, IO);

static TypeMap<HashMap<String, DataStoreBase*>> g_global_data_store_map {};
static Mutex g_global_data_store_mutex;

DataStoreBase* DataStoreBase::GetOrCreate(TypeID data_store_type_id, UTF8StringView prefix, ProcRef<DataStoreBase*(UTF8StringView)>&& create_fn)
{
    Mutex::Guard guard(g_global_data_store_mutex);

    auto it = g_global_data_store_map.Find(data_store_type_id);

    if (it == g_global_data_store_map.End())
    {
        it = g_global_data_store_map.Set(data_store_type_id, {}).first;
    }

    auto data_store_it = it->second.FindAs(prefix);

    if (data_store_it == it->second.End())
    {
        DataStoreBase* create_result = create_fn(prefix);
        AssertThrow(create_result != nullptr);

        data_store_it = it->second.Insert({ prefix, create_result }).first;
    }

    return data_store_it->second;
}

DataStoreBase::DataStoreBase(const String& prefix, DataStoreOptions options)
    : m_prefix(prefix),
      m_options(options)
{
}

int DataStoreBase::IncRef()
{
    return m_ref_counter.Produce(1, [this](bool)
        {
            if (m_options.flags & DSF_WRITE)
            {
                AssertThrowMsg(MakeDirectory(), "Failed to create directory for data store at path %s", GetDirectory().Data());
            }
        });
}

int DataStoreBase::IncRefNoInitialize()
{
    return IncRef();
}

int DataStoreBase::DecRef()
{
    m_shutdown_semaphore.Produce(1);
    bool should_release = true;

    int result = m_ref_counter.Release(1, [this, &should_release](bool)
        {
            should_release = false;
            TaskSystem::GetInstance().Enqueue(
                HYP_STATIC_MESSAGE("DiscardOldFiles on DataStoreBase::DecRef"),
                [this]()
                {
                    DiscardOldFiles();

                    m_shutdown_semaphore.Release(1);
                },
                TaskThreadPoolName::THREAD_POOL_BACKGROUND,
                TaskEnqueueFlags::FIRE_AND_FORGET);
        });

    if (should_release)
    {
        m_shutdown_semaphore.Release(1);
    }

    return result;
}

void DataStoreBase::WaitForTaskCompletion() const
{
    m_shutdown_semaphore.Acquire();
}

void DataStoreBase::WaitForFinalization() const
{
    WaitForTaskCompletion();
    m_ref_counter.Acquire();
}

void DataStoreBase::DiscardOldFiles() const
{
    if (m_options.max_size == 0)
    {
        return; // No limit
    }

    HYP_LOG(DataStore, Debug, "Discarding old files in data store {}", m_prefix);

    const FilePath path = GetDirectory();

    SizeType directory_size = path.DirectorySize();

    if (directory_size <= m_options.max_size)
    {
        return;
    }

    const Time now = Time::Now();

    // Sort by oldest first -- use diff from now
    FlatMap<TimeDiff, FilePath> files_by_time;

    for (const FilePath& file : path.GetAllFilesInDirectory())
    {
        files_by_time.Insert(now - file.LastModifiedTimestamp(), file);
    }

    while (directory_size > m_options.max_size)
    {
        const auto it = files_by_time.Begin();

        if (it == files_by_time.End())
        {
            break;
        }

        const FilePath& file = it->second;

        directory_size -= file.FileSize();

        if (!file.Remove())
        {
            HYP_LOG(DataStore, Warning, "Failed to remove file {}", file);
        }

        files_by_time.Erase(it);
    }
}

bool DataStoreBase::MakeDirectory() const
{
    const FilePath path = GetDirectory();

    if (!path.Exists() || !path.IsDirectory())
    {
        return path.MkDir();
    }

    return true;
}

void DataStoreBase::Write(const String& key, const ByteBuffer& byte_buffer)
{
    AssertThrowMsg(m_ref_counter.IsInSignalState(), "Cannot write to DataStore, not yet init");
    AssertThrowMsg(m_options.flags & DSF_WRITE, "Data store is not writable");

    const FilePath filepath = GetDirectory() / key;

    FileByteWriter writer(filepath.Data());
    writer.Write(byte_buffer.Data(), byte_buffer.Size());
    writer.Close();
}

bool DataStoreBase::Read(const String& key, ByteBuffer& out_byte_buffer) const
{
    AssertThrowMsg(m_ref_counter.IsInSignalState(), "Cannot read from DataStore, not yet init");
    AssertThrowMsg(m_options.flags & DSF_READ, "Data store is not readable");

    const FilePath directory = GetDirectory();

    if (!directory.Exists() || !directory.IsDirectory())
    {
        return false;
    }

    const FilePath filepath = directory / key;

    if (!filepath.Exists())
    {
        return false;
    }

    FileBufferedReaderSource source { filepath };
    BufferedByteReader reader { &source };

    if (!reader.IsOpen())
    {
        HYP_LOG(DataStore, Warning, "Could not open file at path {} for reading", filepath);

        return false;
    }

    out_byte_buffer = reader.ReadBytes();

    return true;
}

bool DataStoreBase::Exists(const String& key) const
{
    AssertThrowMsg(m_ref_counter.IsInSignalState(), "Cannot read from DataStore, not yet init");
    AssertThrowMsg(m_options.flags & DSF_READ, "Data store is not readable");

    const FilePath directory = GetDirectory();

    if (!directory.Exists() || !directory.IsDirectory())
    {
        return false;
    }

    const FilePath filepath = directory / key;

    return filepath.Exists();
}

FilePath DataStoreBase::GetDirectory() const
{
    return GetResourceDirectory() / "data" / m_prefix;
}

} // namespace filesystem
} // namespace hyperion
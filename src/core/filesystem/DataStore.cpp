/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/filesystem/DataStore.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/utilities/Time.hpp>

#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>

namespace hyperion {
namespace filesystem {

static TypeMap<HashMap<String, DataStoreBase *>> g_global_data_store_map { };
static Mutex g_global_data_store_mutex;

DataStoreBase *DataStoreBase::GetOrCreate(TypeID data_store_type_id, UTF8StringView prefix, ProcRef<DataStoreBase *, UTF8StringView> &&create_fn)
{
    Mutex::Guard guard(g_global_data_store_mutex);

    auto it = g_global_data_store_map.Find(data_store_type_id);

    if (it == g_global_data_store_map.End()) {
        it = g_global_data_store_map.Set(data_store_type_id, { }).first;
    }

    auto data_store_it = it->second.FindAs(prefix);

    if (data_store_it == it->second.End()) {
        DataStoreBase *create_result = create_fn(prefix);
        AssertThrow(create_result != nullptr);
        
        data_store_it = it->second.Insert({ prefix, create_result }).first;
    }

    return data_store_it->second;
}

DataStoreBase::DataStoreBase(const String &prefix, DataStoreOptions options)
    : m_prefix(prefix),
      m_options(options)
{
}

int DataStoreBase::Claim()
{
    return m_init_semaphore.Produce(1, [this](bool)
    {
        if (m_options.flags & DSF_WRITE) {
            AssertThrowMsg(MakeDirectory(), "Failed to create directory for data store at path %s", GetDirectory().Data());
        }
    });
}

int DataStoreBase::Unclaim()
{
    return m_init_semaphore.Release(1, [this](bool)
    {
        m_shutdown_semaphore.Produce(1);

        TaskSystem::GetInstance().Enqueue([this]()
        {
            DiscardOldFiles();

            m_shutdown_semaphore.Release(1);
        }, TaskThreadPoolName::THREAD_POOL_GENERIC, TaskEnqueueFlags::FIRE_AND_FORGET);
    });
}

void DataStoreBase::WaitForCompletion()
{
    m_shutdown_semaphore.Acquire();
}

void DataStoreBase::DiscardOldFiles() const
{
    if (m_options.max_size == 0) {
        return; // No limit
    }

    const FilePath path = GetDirectory();

    SizeType directory_size = path.DirectorySize();

    if (directory_size <= m_options.max_size) {
        return;
    }

    const Time now = Time::Now();

    // Sort by oldest first -- use diff from now
    FlatMap<TimeDiff, FilePath> files_by_time;

    for (const FilePath &file : path.GetAllFilesInDirectory()) {
        files_by_time.Insert(now - file.LastModifiedTimestamp(), file);
    }

    while (directory_size > m_options.max_size) {
        const auto it = files_by_time.Begin();

        if (it == files_by_time.End()) {
            break;
        }

        const FilePath &file = it->second;

        directory_size -= file.FileSize();

        if (!file.Remove()) {
            DebugLog(
                LogType::Warn,
                "Failed to remove file %s\n",
                file.Data()
            );
        }

        files_by_time.Erase(it);
    }
}

bool DataStoreBase::MakeDirectory() const
{
    const FilePath path = GetDirectory();

    if (!path.Exists() || !path.IsDirectory()) {
        return path.MkDir();
    }

    return true;
}

void DataStoreBase::Write(const String &key, const ByteBuffer &byte_buffer)
{
    AssertThrowMsg(m_init_semaphore.IsInSignalState(), "Cannot write to DataStore, not yet init");
    AssertThrowMsg(m_options.flags & DSF_WRITE, "Data store is not writable");

    const FilePath filepath = GetDirectory() / key;

    FileByteWriter writer(filepath.Data());
    writer.Write(byte_buffer.Data(), byte_buffer.Size());
    writer.Close();
}

bool DataStoreBase::Read(const String &key, ByteBuffer &out_byte_buffer) const
{
    AssertThrowMsg(m_init_semaphore.IsInSignalState(), "Cannot read from DataStore, not yet init");
    AssertThrowMsg(m_options.flags & DSF_READ, "Data store is not readable");

    const FilePath directory = GetDirectory();

    if (!directory.Exists() || !directory.IsDirectory()) {
        return false;
    }

    const FilePath filepath = directory / key;

    if (!filepath.Exists()) {
        return false;
    }
    
    BufferedByteReader reader(filepath);
    out_byte_buffer = reader.ReadBytes();

    return true;
}

bool DataStoreBase::Exists(const String &key) const
{
    AssertThrowMsg(m_init_semaphore.IsInSignalState(), "Cannot read from DataStore, not yet init");
    AssertThrowMsg(m_options.flags & DSF_READ, "Data store is not readable");

    const FilePath directory = GetDirectory();

    if (!directory.Exists() || !directory.IsDirectory()) {
        return false;
    }

    const FilePath filepath = directory / key;

    return filepath.Exists();
}

FilePath DataStoreBase::GetDirectory() const
{
    return AssetManager::GetInstance()->GetBasePath() / "data" / m_prefix;
}

} // namespace filesystem
} // namespace hyperion
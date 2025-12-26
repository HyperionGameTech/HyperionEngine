/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/filesystem/DataStore.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/utilities/Time.hpp>

#include <core/io/ByteWriter.hpp>
#include <core/io/BufferedByteReader.hpp>
#include <core/io/ByteReader.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace Hyperion {

extern FilePath CoreApi_GetExecutablePath();

namespace filesystem {

HYP_DEFINE_LOG_SUBCHANNEL(DataStore, IO);

static TypeMap<HashMap<String, DataStoreBase*>> s_globalDataStoreMap {};
static Mutex s_globalDataStoreMutex;

DataStoreBase* DataStoreBase::GetOrCreate(TypeId dataStoreTypeId, UTF8StringView prefix, ProcRef<DataStoreBase*(UTF8StringView)>&& createFn)
{
    Mutex::Guard guard(s_globalDataStoreMutex);

    auto it = s_globalDataStoreMap.Find(dataStoreTypeId);

    if (it == s_globalDataStoreMap.End())
    {
        it = s_globalDataStoreMap.Set(dataStoreTypeId, {}).first;
    }

    auto dataStoreIt = it->second.FindAs(prefix);

    if (dataStoreIt == it->second.End())
    {
        DataStoreBase* createResult = createFn(prefix);
        HYP_CORE_ASSERT(createResult != nullptr);

        dataStoreIt = it->second.Insert({ prefix, createResult }).first;
    }

    return dataStoreIt->second;
}

DataStoreBase::DataStoreBase(const String& prefix, DataStoreOptions options)
    : m_prefix(prefix),
      m_options(options)
{
}

int DataStoreBase::IncRef()
{
    return m_refCounter.Produce(1, [this](bool)
        {
            if (m_options.flags & DSF_WRITE)
            {
                const bool result = MakeDirectory();

                HYP_CORE_ASSERT(result, "Failed to create directory for data store at path %s", GetDirectory().Data());
            }
        });
}

int DataStoreBase::DecRef()
{
    m_shutdownSemaphore.Produce(1);
    bool shouldRelease = true;

    int result = m_refCounter.Release(1, [this, &shouldRelease](bool)
        {
            shouldRelease = false;
            TaskSystem::GetInstance().Enqueue(
                HYP_STATIC_MESSAGE("DiscardOldFiles on DataStoreBase::DecRef"),
                [this]()
                {
                    DiscardOldFiles();

                    m_shutdownSemaphore.Release(1);
                },
                TaskThreadPoolName::THREAD_POOL_BACKGROUND,
                TaskEnqueueFlags::FIRE_AND_FORGET);
        });

    if (shouldRelease)
    {
        m_shutdownSemaphore.Release(1);
    }

    return result;
}

void DataStoreBase::WaitForFinalization() const
{
    m_shutdownSemaphore.Acquire();
    m_refCounter.Acquire();
}

void DataStoreBase::DiscardOldFiles() const
{
    if (m_options.maxSize == 0)
    {
        return; // No limit
    }

    HYP_LOG(DataStore, Debug, "Discarding old files in data store {}", m_prefix);

    const FilePath path = GetDirectory();

    SizeType directorySize = path.DirectorySize();

    if (directorySize <= m_options.maxSize)
    {
        return;
    }

    const Time now = Time::Now();

    // Sort by oldest first -- use diff from now
    FlatMap<TimeDiff, FilePath> filesByTime;

    for (const FilePath& file : path.GetAllFilesInDirectory())
    {
        filesByTime.Insert(now - file.LastModifiedTimestamp(), file);
    }

    while (directorySize > m_options.maxSize)
    {
        const auto it = filesByTime.Begin();

        if (it == filesByTime.End())
        {
            break;
        }

        const FilePath& file = it->second;

        directorySize -= file.FileSize();

        if (!file.Remove())
        {
            HYP_LOG(DataStore, Warning, "Failed to remove file {}", file);
        }

        filesByTime.Erase(it);
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

void DataStoreBase::Write(const String& key, const ByteBuffer& byteBuffer)
{
    HYP_CORE_ASSERT(m_refCounter.IsInSignalState(), "Cannot write to DataStore, not yet init");
    HYP_CORE_ASSERT(m_options.flags & DSF_WRITE, "Data store is not writable");

    const FilePath filepath = GetDirectory() / key;

    FileByteWriter writer(filepath.Data());
    writer.Write(byteBuffer.Data(), byteBuffer.Size());
    writer.Close();
}

bool DataStoreBase::Read(const String& key, ByteBuffer& outByteBuffer) const
{
    HYP_CORE_ASSERT(m_refCounter.IsInSignalState(), "Cannot read from DataStore, not yet init");
    HYP_CORE_ASSERT(m_options.flags & DSF_READ, "Data store is not readable");

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

    outByteBuffer = reader.ReadBytes();

    return true;
}

bool DataStoreBase::Exists(const String& key) const
{
    HYP_CORE_ASSERT(m_refCounter.IsInSignalState(), "Cannot read from DataStore, not yet init");
    HYP_CORE_ASSERT(m_options.flags & DSF_READ, "Data store is not readable");

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
    return CoreApi_GetExecutablePath() / "data" / m_prefix;
}

} // namespace filesystem
} // namespace Hyperion

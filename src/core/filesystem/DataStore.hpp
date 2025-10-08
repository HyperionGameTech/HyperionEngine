/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/filesystem/FilePath.hpp>

#include <core/containers/String.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/memory/resource/Resource.hpp>

#include <core/threading/Semaphore.hpp>

#include <core/functional/Proc.hpp>

namespace hyperion {

using DataStoreFlags = uint32;

enum DataStoreFlagBits : DataStoreFlags
{
    DSF_NONE = 0x0,
    DSF_READ = 0x1,
    DSF_WRITE = 0x2,
    DSF_RW = DSF_READ | DSF_WRITE
};

namespace filesystem {

struct DataStoreOptions
{
    DataStoreFlags flags = DSF_RW;
    // max size in bytes before old data is discarded - 0 means no limit
    uint64 maxSize = 5ull * 1024ull * 1024ull * 1024ull /* 5GB */;
};

class HYP_API DataStoreBase : public IResource
{
    using RefCounter = Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::ConditionVarSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>;
    using ShutdownSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::ConditionVarSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

public:
    static DataStoreBase* GetOrCreate(TypeId dataStoreTypeId, UTF8StringView prefix, ProcRef<DataStoreBase*(UTF8StringView)>&& createFn);

    template <class DataStoreType>
    static DataStoreType& GetOrCreate(UTF8StringView prefix)
    {
        static_assert(std::is_base_of_v<DataStoreBase, DataStoreType>, "DataStoreType must be a subclass of DataStoreBase");

        DataStoreBase* ptr = DataStoreBase::GetOrCreate(TypeId::ForType<DataStoreType>(), prefix, [](UTF8StringView prefix) -> DataStoreBase*
            {
                return AllocateResource<DataStoreType>(prefix);
            });

        DataStoreType* ptrCasted = dynamic_cast<DataStoreType*>(ptr);
        HYP_CORE_ASSERT(ptrCasted != nullptr);

        return *ptrCasted;
    }

    DataStoreBase(const String& prefix, DataStoreOptions options);
    DataStoreBase(const DataStoreBase& other) = delete;
    DataStoreBase& operator=(const DataStoreBase& other) = delete;
    DataStoreBase(DataStoreBase&& other) = default;
    DataStoreBase& operator=(DataStoreBase&& other) = delete;
    virtual ~DataStoreBase() override = default;

    /*! \brief Discard old files if the directory size exceeds the max size.
     *  \todo Call this function on cleanup, before exit. Make this run in a separate thread (with a lock on the directory) */
    void DiscardOldFiles() const;

    /*! \brief Get the directory path for the data store
     *  \returns The directory path as a FilePath */
    virtual FilePath GetDirectory() const;

    /*! \brief Create the directory for the data store, if it doesn't already exist
     *  \returns true if the directory was created or already exists, false otherwise */
    bool MakeDirectory() const;

    /*! \brief Write a byte buffer keyed by \ref{key} to the data store
     *  \param key The key to use for the data that will be written
     *  \param byteBuffer The byte buffer to write */
    virtual void Write(const String& key, const ByteBuffer& byteBuffer);

    /*! \brief Read a byte buffer keyed by \ref{key} from the data store
     *  \param key The key to use for the data that will be read
     *  \param outByteBuffer The byte buffer to read into
     *  \returns true if the read was successful, false otherwise */
    virtual bool Read(const String& key, ByteBuffer& outByteBuffer) const;

    /*! \brief Check if a key exists in the data store
     *  \param key The key to check for
     *  \returns true if the key exists, false otherwise */
    virtual bool Exists(const String& key) const;

    virtual ResourceMemoryPoolHandle GetPoolHandle() const override
    {
        return m_poolHandle;
    }

    virtual void SetPoolHandle(ResourceMemoryPoolHandle poolHandle) override
    {
        m_poolHandle = poolHandle;
    }

protected:
    virtual bool IsNull() const override
    {
        return false;
    }

    virtual int IncRef() override;
    virtual int IncRefNoInitialize() override;
    virtual int DecRef() override;

    virtual void WaitForFinalization() const override final;

private:
    ResourceMemoryPoolHandle m_poolHandle;
    String m_prefix;
    DataStoreOptions m_options;
    RefCounter m_refCounter;
    ShutdownSemaphore m_shutdownSemaphore;
};

class DataStore : public DataStoreBase
{
public:
    DataStore(UTF8StringView prefix, DataStoreOptions options)
        : DataStoreBase(prefix, options)
    {
    }

    virtual ~DataStore() override = default;
};

class ReadOnlyDataStore : public DataStore
{
public:
    ReadOnlyDataStore(UTF8StringView prefix)
        : DataStore(prefix, DataStoreOptions { DSF_READ })
    {
    }

    virtual ~ReadOnlyDataStore() override = default;
};

class ReadWriteDataStore : public DataStore
{
public:
    ReadWriteDataStore(UTF8StringView prefix)
        : DataStore(prefix, DataStoreOptions { DSF_RW })
    {
    }

    virtual ~ReadWriteDataStore() override = default;
};

template <auto Prefix, DataStoreFlags Flags>
static inline DataStore& GetDataStore()
{
    if constexpr (Flags & DSF_WRITE)
    {
        return DataStoreBase::GetOrCreate<ReadWriteDataStore>(&Prefix.data[0]);
    }
    else if constexpr (Flags & DSF_READ)
    {
        return DataStoreBase::GetOrCreate<ReadOnlyDataStore>(&Prefix.data[0]);
    }
    else
    {
        static_assert(ResolutionFailureV<std::integral_constant<int, int(Flags)>>, "Cannot create DataStore with the given flags!");
    }
}

} // namespace filesystem

using filesystem::DataStore;
using filesystem::DataStoreBase;
using filesystem::DataStoreOptions;
using filesystem::GetDataStore;

} // namespace hyperion

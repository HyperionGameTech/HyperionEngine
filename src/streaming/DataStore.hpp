/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DATA_STORE_HPP
#define HYPERION_DATA_STORE_HPP

#include <core/filesystem/FilePath.hpp>
#include <core/containers/String.hpp>
#include <core/memory/ByteBuffer.hpp>

namespace hyperion {

using DataStoreFlags = uint32;

enum DataStoreFlagBits : DataStoreFlags
{
    DSF_NONE    = 0x0,
    DSF_READ    = 0x1,
    DSF_WRITE   = 0x2,
    DSF_RW      = DSF_READ | DSF_WRITE
};

struct DataStoreOptions
{
    DataStoreFlags  flags = DSF_RW;
    // max size in bytes before old data is discarded - 0 means no limit
    uint64          max_size = 5ull * 1024ull * 1024ull * 1024ull /* 5GB */;
};

class HYP_API DataStoreBase
{
public:
    DataStoreBase(const String &prefix, DataStoreOptions options);
    DataStoreBase(const DataStoreBase &other)               = delete;
    DataStoreBase &operator=(const DataStoreBase &other)    = delete;
    DataStoreBase(DataStoreBase &&other)                    = default;
    DataStoreBase &operator=(DataStoreBase &&other)         = default;
    virtual ~DataStoreBase()                                = default;

    /*! \brief Discard old files if the directory size exceeds the max size.
     *  \todo Call this function on cleanup, before exit. Make this run in a separate thread (with a lock on the directory) */
    void DiscardOldFiles() const;

    /*! \brief Get the directory path for the data store
     *  \returns The directory path as a FilePath */
    FilePath GetDirectory() const;

    /*! \brief Create the directory for the data store, if it doesn't already exist
     *  \returns true if the directory was created or already exists, false otherwise */
    bool MakeDirectory() const;

    /*! \brief Write a byte buffer keyed by \ref{key} to the data store
     *  \param key The key to use for the data that will be written
     *  \param byte_buffer The byte buffer to write */
    virtual void Write(const String &key, const ByteBuffer &byte_buffer);

    /*! \brief Read a byte buffer keyed by \ref{key} from the data store
     *  \param key The key to use for the data that will be read
     *  \param out_byte_buffer The byte buffer to read into
     *  \returns true if the read was successful, false otherwise */
    virtual bool Read(const String &key, ByteBuffer &out_byte_buffer) const;

private:
    String              m_prefix;
    DataStoreOptions    m_options;
};

template <auto Prefix, DataStoreFlags Flags, uint64 MaxSize = 1ull * 1024ull * 1024ull * 1024ull>
class DataStore : public DataStoreBase
{
public:
    DataStore()
        : DataStoreBase(&Prefix.data[0], DataStoreOptions { Flags, MaxSize })
    {
    }

    virtual ~DataStore() override = default;
};

template <auto Prefix, DataStoreFlags Flags>
static inline DataStore<Prefix, Flags> &GetDataStore()
{
    static DataStore<Prefix, Flags> data_store;

    return data_store;
}


} // namespace hyperion

#endif
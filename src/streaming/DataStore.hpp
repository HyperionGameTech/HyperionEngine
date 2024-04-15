/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_V2_DATA_STORE_HPP
#define HYPERION_V2_DATA_STORE_HPP

#include <util/fs/FsUtil.hpp>
#include <core/lib/String.hpp>
#include <core/lib/ByteBuffer.hpp>

namespace hyperion::v2 {

using DataStoreFlags = uint32;

enum DataStoreFlagBits : DataStoreFlags
{
    DATA_STORE_FLAG_NONE    = 0x0,
    DATA_STORE_FLAG_READ    = 0x1,
    DATA_STORE_FLAG_WRITE   = 0x2,
    DATA_STORE_FLAG_RW      = DATA_STORE_FLAG_READ | DATA_STORE_FLAG_WRITE
};

struct DataStoreOptions
{
    DataStoreFlags  flags = DATA_STORE_FLAG_RW;
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

    void DiscardOldFiles() const;
    FilePath GetDirectory() const;
    bool MakeDirectory() const;

    virtual void Write(const String &key, const ByteBuffer &byte_buffer);
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


} // namespace hyperion::v2

#endif
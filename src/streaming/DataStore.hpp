#ifndef HYPERION_V2_DATA_STORE_HPP
#define HYPERION_V2_DATA_STORE_HPP

#include <util/fs/FsUtil.hpp>
#include <core/lib/String.hpp>
#include <core/lib/ByteBuffer.hpp>

namespace hyperion::v2 {

class DataStoreBase
{
public:
    DataStoreBase(const String &prefix);
    DataStoreBase(const DataStoreBase &other) = delete;
    DataStoreBase &operator=(const DataStoreBase &other) = delete;
    DataStoreBase(DataStoreBase &&other) = default;
    DataStoreBase &operator=(DataStoreBase &&other) = default;
    virtual ~DataStoreBase() = default;

    virtual FilePath GetDirectory() const;

    virtual bool MakeDirectory() const;

    virtual void Write(const String &key, const ByteBuffer &byte_buffer);
    virtual bool Read(const String &key, ByteBuffer &out_byte_buffer) const;

private:
    String m_prefix;
};

template <auto Prefix>
class DataStore : public DataStoreBase
{
public:
    DataStore()
        : DataStoreBase(&Prefix.data[0])
    {
    }
};

template <auto Prefix>
static inline DataStore<Prefix> &GetDataStore()
{
    static DataStore<Prefix> data_store;

    return data_store;
}


} // namespace hyperion::v2

#endif
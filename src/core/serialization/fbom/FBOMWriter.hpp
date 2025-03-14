/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_WRITER_HPP
#define HYPERION_FBOM_WRITER_HPP

#include <core/containers/TypeMap.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/TypeAttributes.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/math/MathUtil.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/serialization/fbom/FBOMObject.hpp>
#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/FBOMType.hpp>
#include <core/serialization/fbom/FBOMStaticData.hpp>
#include <core/serialization/fbom/FBOMObjectLibrary.hpp>
#include <core/serialization/fbom/FBOMEnums.hpp>
#include <core/serialization/fbom/FBOMConfig.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>
#include <cstring>

namespace hyperion {

namespace compression {
class Archive;
} // namespace compression

using compression::Archive;

namespace fbom {

struct FBOMObjectType;

class FBOMObject;
class FBOMReader;
class FBOMWriter;
class FBOMArray;
class FBOMData;
class FBOMLoadContext;

struct FBOMWriteStream
{
    UniqueID                            m_name_table_id;
    Array<FBOMObjectLibrary>            m_object_libraries;
    HashMap<UniqueID, FBOMStaticData>   m_static_data;    // map hashcodes to static data to be stored.
    bool                                m_is_writing_static_data = false; // is writing to static data locked? (prevents iterator invalidation)
    SizeType                            m_static_data_offset = 0;
    FlatMap<UniqueID, int>              m_hash_use_count_map;
    Array<FBOMObject>                   m_object_data;
    bool                                m_object_data_write_locked = false; // is writing to object data locked? (prevents iterator invalidation)
    FBOMResult                          m_last_result = FBOMResult::FBOM_OK;

    FBOMWriteStream();
    FBOMWriteStream(const FBOMWriteStream &other)                   = default;
    FBOMWriteStream &operator=(const FBOMWriteStream &other)        = default;
    FBOMWriteStream(FBOMWriteStream &&other) noexcept               = default;
    FBOMWriteStream &operator=(FBOMWriteStream &&other) noexcept    = default;
    ~FBOMWriteStream()                                              = default;

    FBOMDataLocation GetDataLocation(const UniqueID &unique_id, const FBOMStaticData **out_static_data, const FBOMExternalObjectInfo **out_external_object_info) const;

    HYP_FORCE_INLINE void BeginStaticDataWriting()
        { m_is_writing_static_data = true; }

    HYP_FORCE_INLINE void EndStaticDataWriting()
        { m_is_writing_static_data = false; }

    HYP_FORCE_INLINE bool IsWritingStaticData() const
        { return m_is_writing_static_data; }

    HYP_FORCE_INLINE void LockObjectDataWriting()
        { m_object_data_write_locked = true; }

    HYP_FORCE_INLINE void UnlockObjectDataWriting()
        { m_object_data_write_locked = false; }

    HYP_FORCE_INLINE bool IsObjectDataWritingLocked() const
        { return m_object_data_write_locked; }

    void AddToObjectLibrary(FBOMObject &object);
};

class FBOMWriter
{
public:
    FBOMWriter(const FBOMWriterConfig &config);
    FBOMWriter(const FBOMWriterConfig &config, const RC<FBOMWriteStream> &write_stream);

    FBOMWriter(const FBOMWriter &other)             = delete;
    FBOMWriter &operator=(const FBOMWriter &other)  = delete;
    
    FBOMWriter(FBOMWriter &&other) noexcept;
    FBOMWriter &operator=(FBOMWriter &&other) noexcept;

    ~FBOMWriter();

    HYP_FORCE_INLINE const FBOMWriterConfig &GetConfig() const
        { return m_config; }

    HYP_FORCE_INLINE FBOMWriteStream *GetWriteStream() const
        { return m_write_stream.Get(); }

    template <class T>
    typename std::enable_if_t<!std::is_same_v<NormalizedType<T>, FBOMObject>, FBOMResult>
    Append(const T &in, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        FBOMObject object;

        if (FBOMResult err = FBOMObject::Serialize(in, object, flags)) {
            m_write_stream->m_last_result = err;

            return err;
        }

        return Append(std::move(object));
    }

    FBOMResult Append(const FBOMObject &object);
    FBOMResult Append(FBOMObject &&object);

    FBOMResult Emit(ByteWriter *out, bool write_header = true);

    FBOMResult Write(ByteWriter *out, const FBOMObject &object, UniqueID id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter *out, const FBOMType &type, UniqueID id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter *out, const FBOMData &data, UniqueID id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter *out, const FBOMArray &array, UniqueID id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);

private:
    FBOMResult WriteArchive(ByteWriter *out, const Archive &archive) const;

    FBOMResult WriteDataAttributes(ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes) const;
    FBOMResult WriteDataAttributes(ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes, FBOMDataLocation location) const;

    FBOMResult WriteExternalObjects(ByteWriter *out, const FilePath &base_path, const FilePath &external_path);
    
    FBOMResult BuildStaticData(FBOMLoadContext &context);
    FBOMResult AddExternalObjects(FBOMLoadContext &context, FBOMObject &object);

    FBOMResult WriteHeader(ByteWriter *out);
    FBOMResult WriteStaticData(ByteWriter *out);

    FBOMResult WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &) const;

    void AddObjectData(const FBOMObject &, UniqueID id);
    void AddObjectData(FBOMObject &&, UniqueID id);
    
    UniqueID AddStaticData(FBOMLoadContext &context, const FBOMType &);
    UniqueID AddStaticData(FBOMLoadContext &context, const FBOMObject &);
    UniqueID AddStaticData(FBOMLoadContext &context, const FBOMData &);
    UniqueID AddStaticData(FBOMLoadContext &context, const FBOMArray &);

    UniqueID AddStaticData(UniqueID id, FBOMStaticData &&static_data);

    HYP_FORCE_INLINE UniqueID AddStaticData(FBOMStaticData &&static_data)
    {
        const UniqueID id = static_data.GetUniqueID();
        AssertThrow(id != UniqueID::Invalid());

        return AddStaticData(id, std::move(static_data));
    }

    RC<FBOMWriteStream> m_write_stream;
    FBOMWriterConfig    m_config;
};

} // namespace fbom
} // namespace hyperion

#endif

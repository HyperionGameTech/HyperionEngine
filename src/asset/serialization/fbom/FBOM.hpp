/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_HPP
#define HYPERION_FBOM_HPP

#include <core/containers/TypeMap.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/memory/Any.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <math/MathUtil.hpp>

#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>

#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMType.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>
#include <asset/serialization/fbom/FBOMStaticData.hpp>

#include <util/fs/FsUtil.hpp>

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

enum class FBOMVersionCompareMode : uint32
{
    MAJOR   = 0x1,
    MINOR   = 0x2,
    PATCH   = 0x4,
    
    DEFAULT = uint32(MAJOR) | uint32(MINOR)
};

HYP_MAKE_ENUM_FLAGS(FBOMVersionCompareMode)

enum class FBOMDataLocation : uint8
{
    LOC_STATIC  = 0,
    LOC_INPLACE,
    LOC_EXT_REF
};

namespace fbom {

struct FBOMObjectType;

class FBOMObject;
class FBOMReader;
class FBOMWriter;
class FBOMArray;

struct FBOMVersion
{
    uint32  value;

    constexpr FBOMVersion()
        : value(0)
    {
    }

    constexpr FBOMVersion(uint32 value)
        : value(value)
    {
    }

    constexpr FBOMVersion(uint8 major, uint8 minor, uint8 patch)
        : value((uint32(major) << 16) | (uint32(minor) << 8) | (uint32(patch)))
          
    {
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 GetMajor() const
        { return (value & (0xffu << 16)) >> 16; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 GetMinor() const
        { return (value & (0xffu << 8)) >> 8; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 GetPatch() const
        { return value & 0xffu; }

    /*! \brief Returns an integer indicating whether the two version are compatible or not.
     *  If the returned value is equal to zero, the two versions are compatible.
     *  If the returned value is less than zero, \ref{lhs} is incompatible, due to being outdated.
     *  If the returned value is less than zero, \ref{lhs} is incompatible, due to being newer. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    static int TestCompatibility(const FBOMVersion &lhs, const FBOMVersion &rhs, EnumFlags<FBOMVersionCompareMode> compare_mode = FBOMVersionCompareMode::DEFAULT)
    {
        if (compare_mode & FBOMVersionCompareMode::MAJOR) {
            if (lhs.GetMajor() < rhs.GetMajor()) {
                return -1;
            } else if (lhs.GetMajor() > rhs.GetMajor()) {
                return 1;
            }
        }

        if (compare_mode & FBOMVersionCompareMode::MINOR) {
            if (lhs.GetMinor() < rhs.GetMinor()) {
                return -1;
            } else if (lhs.GetMinor() > rhs.GetMinor()) {
                return 1;
            }
        }

        if (compare_mode & FBOMVersionCompareMode::PATCH) {
            if (lhs.GetPatch() < rhs.GetPatch()) {
                return -1;
            } else if (lhs.GetPatch() > rhs.GetPatch()) {
                return 1;
            }
        }

        return 0;
    }
};


enum FBOMCommand : uint8
{
    FBOM_NONE = 0,
    FBOM_OBJECT_START,
    FBOM_OBJECT_END,
    FBOM_STATIC_DATA_START,
    FBOM_STATIC_DATA_END,
    FBOM_DEFINE_PROPERTY
};

class HYP_API FBOM
{
public:
    static constexpr SizeType header_size = 32;
    static constexpr char header_identifier[] = { 'H', 'Y', 'P', '\0' };
    static constexpr FBOMVersion version = FBOMVersion { 1, 7, 0 };

    static FBOM &GetInstance();

    FBOM();
    FBOM(const FBOM &other)             = delete;
    FBOM &operator=(const FBOM &other)  = delete;
    ~FBOM();
    
    void RegisterLoader(TypeID type_id, UniquePtr<FBOMMarshalerBase> &&marshal);

    /*! \brief Get a registered loader for the given object type.
     *  \example Use the type MyStruct to get a registered instance of FBOMMarshaler<MyStruct>
     *  \tparam T The type of the class to get the loader for
     *  \return A pointer to the loader instance, or nullptr if no loader is registered for the given type
     */
    template <class T>
    [[nodiscard]] HYP_FORCE_INLINE FBOMMarshalerBase *GetLoader() const
        { return GetLoader(TypeID::ForType<NormalizedType<T>>()); }

    /*! \brief Get a registered loader for the given object TypeID.
     *  \example Use the TypeID for MyStruct to get a registered instance of FBOMMarshaler<MyStruct>
     *  \param object_type_id The TypeID of the class to get the loader for
     *  \return A pointer to the loader instance, or nullptr if no loader is registered for the given TypeID
     */
    [[nodiscard]] FBOMMarshalerBase *GetLoader(TypeID object_type_id) const;

    /*! \brief Get a registered loader for the given object type name.
     *  \example Use the string "MyStruct" to get a registered instance of FBOMMarshaler<MyStruct> (assuming GetObjectType() has not been overridden in the marshaler class)
     *  \param type_name The name of the class to get the loader for
     *  \return A pointer to the loader instance, or nullptr if no loader is registered for the given type name
     */
    [[nodiscard]] FBOMMarshalerBase *GetLoader(const ANSIStringView &type_name) const;

private:
    FlatMap<ANSIString, UniquePtr<FBOMMarshalerBase>>   m_marshals;
};

struct FBOMConfig
{
    bool                                        continue_on_external_load_error = false;
    String                                      base_path;
    FlatMap<Pair<String, uint32>, FBOMObject>   external_data_cache;
};

class FBOMReader
{
public:
    FBOMReader(const FBOMConfig &config);
    ~FBOMReader();

    FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out_object);
    FBOMResult Deserialize(BufferedReader &reader, FBOMDeserializedObject &out_object);
    FBOMResult Deserialize(BufferedReader &reader, FBOMObject &out);

    FBOMResult LoadFromFile(const String &path, FBOMObject &out);
    FBOMResult LoadFromFile(const String &path, FBOMDeserializedObject &out);

    FBOMResult ReadObject(BufferedReader *, FBOMObject &out_object, FBOMObject *root);
    FBOMResult ReadObjectType(BufferedReader *, FBOMType &out_type);
    FBOMResult ReadData(BufferedReader *, FBOMData &out_data);
    FBOMResult ReadArray(BufferedReader *, FBOMArray &out_array);
    FBOMResult ReadNameTable(BufferedReader *, FBOMNameTable &out_name_table);
    FBOMResult ReadPropertyName(BufferedReader *, Name &out_property_name);

private:
    template <class T>
    void CheckEndianness(T &value)
    {
        static_assert(IsPODType<T>, "T must be POD to use CheckEndianness()");

        if constexpr (sizeof(T) == 1) {
            return;
        } else if (m_swap_endianness) {
            SwapEndianness(value);
        }
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool HasCustomLoaderForType(const FBOMType &type) const
        { return FBOM::GetInstance().GetLoader(type.name) != nullptr; }

    FBOMCommand NextCommand(BufferedReader *);
    FBOMCommand PeekCommand(BufferedReader *);
    FBOMResult Eat(BufferedReader *, FBOMCommand, bool read = true);

    FBOMResult ReadDataAttributes(BufferedReader *, EnumFlags<FBOMDataAttributes> &out_attributes, FBOMDataLocation &out_location);

    template <class StringType>
    FBOMResult ReadString(BufferedReader *, StringType &out_string);

    FBOMResult ReadArchive(BufferedReader *, Archive &out_archive);
    FBOMResult ReadArchive(const ByteBuffer &in_buffer, ByteBuffer &out_buffer);

    FBOMResult ReadRawData(BufferedReader *, SizeType count, ByteBuffer &out_buffer);

    template <class T>
    FBOMResult ReadRawData(BufferedReader *reader, T *out_ptr)
    {
        static_assert(IsPODType<NormalizedType<T>>, "T must be POD to read as raw data");

        AssertThrow(out_ptr != nullptr);

        constexpr SizeType size = sizeof(NormalizedType<T>);

        ByteBuffer byte_buffer;

        if (FBOMResult err = ReadRawData(reader, size, byte_buffer)) {
            return err;
        }

        Memory::MemCpy(static_cast<void *>(out_ptr), static_cast<const void *>(byte_buffer.Data()), size);

        CheckEndianness(*out_ptr);

        return FBOMResult::FBOM_OK;
    }

    FBOMResult Handle(BufferedReader *, FBOMCommand, FBOMObject *root);

    FBOMConfig              m_config;

    bool                    m_in_static_data;
    Array<FBOMStaticData>   m_static_data_pool;

    bool                    m_swap_endianness;
};

struct FBOMExternalData
{
    FlatMap<UniqueID, FBOMObject> objects;
};

struct FBOMWriteStream
{
    // FBOMNameTable                       m_name_table;
    UniqueID                            m_name_table_id;
    FlatMap<String, FBOMExternalData>   m_external_objects; // map filepath -> external object
    HashMap<UniqueID, FBOMStaticData>   m_static_data;    // map hashcodes to static data to be stored.
    bool                                m_static_data_write_locked = false; // is writing to static data locked? (prevents iterator invalidation)
    FlatMap<UniqueID, int>              m_hash_use_count_map;
    Array<FBOMObject>                   m_object_data;// TODO: make multiple root objects be supported by the loader
    SizeType                            m_static_data_offset = 0;
    FBOMResult                          m_last_result = FBOMResult::FBOM_OK;

    FBOMWriteStream();
    FBOMWriteStream(const FBOMWriteStream &other)                   = default;
    FBOMWriteStream &operator=(const FBOMWriteStream &other)        = default;
    FBOMWriteStream(FBOMWriteStream &&other) noexcept               = default;
    FBOMWriteStream &operator=(FBOMWriteStream &&other) noexcept    = default;
    ~FBOMWriteStream()                                              = default;

    FBOMDataLocation GetDataLocation(const UniqueID &unique_id, const FBOMStaticData **out_static_data, String &out_external_key) const;
    void MarkStaticDataWritten(const UniqueID &unique_id);

    HYP_FORCE_INLINE
    void LockStaticDataWriting()
        { m_static_data_write_locked = true; }

    HYP_FORCE_INLINE
    void UnlockStaticDataWriting()
        { m_static_data_write_locked = false; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsStaticDataWritingLocked() const
        { return m_static_data_write_locked; }

    HYP_NODISCARD HYP_FORCE_INLINE
    FBOMNameTable &GetNameTable()
    {
        auto it = m_static_data.Find(m_name_table_id);
        AssertThrow(it != m_static_data.End());

        FBOMNameTable *name_table_ptr = it->second.data.TryGetAsDynamic<FBOMNameTable>();
        AssertThrow(name_table_ptr != nullptr);

        return *name_table_ptr;
    }
};

class FBOMWriter
{
public:
    FBOMWriter();
    FBOMWriter(const RC<FBOMWriteStream> &write_stream);

    FBOMWriter(const FBOMWriter &other)             = delete;
    FBOMWriter &operator=(const FBOMWriter &other)  = delete;
    
    FBOMWriter(FBOMWriter &&other) noexcept;
    FBOMWriter &operator=(FBOMWriter &&other) noexcept;

    ~FBOMWriter();

    const FBOMWriteStream *GetWriteStream() const
        { return m_write_stream.Get(); }

    template <class T>
    typename std::enable_if_t<!std::is_same_v<NormalizedType<T>, FBOMObject>, FBOMResult>
    Append(const T &object, EnumFlags<FBOMObjectFlags> flags = FBOMObjectFlags::NONE)
    {
        FBOMMarshalerBase *marshal = FBOM::GetInstance().GetLoader<NormalizedType<T>>();
        AssertThrowMsg(marshal != nullptr, "No registered marshal class for type: %s", TypeNameWithoutNamespace<NormalizedType<T>>().Data());

        FBOMObjectMarshalerBase<NormalizedType<T>> *marshal_derived = dynamic_cast<FBOMObjectMarshalerBase<NormalizedType<T>> *>(marshal);
        AssertThrowMsg(marshal_derived != nullptr, "Marshal class type mismatch for type %s", TypeNameWithoutNamespace<NormalizedType<T>>().Data());

        FBOMObject base(marshal_derived->GetObjectType());
        base.GenerateUniqueID(object, flags);

        if (FBOMResult err = marshal_derived->Serialize(object, base)) {
            m_write_stream->m_last_result = err;

            return err;
        }

        return Append(base);
    }

    FBOMResult Append(const FBOMObject &object);
    FBOMResult Emit(ByteWriter *out);

    FBOMResult Write(ByteWriter *out, const FBOMObject &object, UniqueID id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter *out, const FBOMType &type, UniqueID id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter *out, const FBOMData &data, UniqueID id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter *out, const FBOMArray &array, UniqueID id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);
    FBOMResult Write(ByteWriter *out, const FBOMNameTable &name_table, UniqueID id, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE);

private:
    FBOMResult WriteArchive(ByteWriter *out, const Archive &archive) const;

    FBOMResult WriteDataAttributes(ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes) const;
    FBOMResult WriteDataAttributes(ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes, FBOMDataLocation location) const;

    FBOMResult WriteExternalObjects();
    void BuildStaticData();
    void Prune(const FBOMObject &);

    FBOMResult WriteHeader(ByteWriter *out);
    FBOMResult WriteStaticData(ByteWriter *out);

    FBOMResult WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &) const;

    void AddObjectData(const FBOMObject &, UniqueID id);
    
    UniqueID AddStaticData(const FBOMType &);
    UniqueID AddStaticData(const FBOMObject &);
    UniqueID AddStaticData(const FBOMData &);
    UniqueID AddStaticData(const FBOMArray &);
    UniqueID AddStaticData(const FBOMNameTable &);

    RC<FBOMWriteStream> m_write_stream;

    UniqueID AddStaticData(UniqueID id, FBOMStaticData &&static_data);

    HYP_FORCE_INLINE
    UniqueID AddStaticData(FBOMStaticData &&static_data)
    {
        const UniqueID id = static_data.GetUniqueID();

        return AddStaticData(id, std::move(static_data));
    }
};
HYP_ENABLE_OPTIMIZATION;

} // namespace fbom
} // namespace hyperion

#endif

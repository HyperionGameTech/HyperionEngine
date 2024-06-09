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
#include <core/ClassInfo.hpp>

#include <math/MathUtil.hpp>

#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>

#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMType.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMLoadable.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>

#include <util/fs/FsUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>
#include <cstring>

namespace hyperion {

enum class FBOMVersionCompareMode : uint32
{
    MAJOR   = 0x1,
    MINOR   = 0x2,
    PATCH   = 0x4,
    
    DEFAULT = uint32(MAJOR) | uint32(MINOR)
};

HYP_MAKE_ENUM_FLAGS(FBOMVersionCompareMode)

enum class FBOMStaticDataFlags : uint32
{
    NONE    = 0x0,
    WRITTEN = 0x1
};

HYP_MAKE_ENUM_FLAGS(FBOMStaticDataFlags)

namespace fbom {

struct FBOMObjectType;

class FBOMObject;
class FBOMReader;
class FBOMWriter;

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
        { return value & (0xffu << 16); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 GetMinor() const
        { return value & (0xffu << 8); }

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

enum FBOMDataLocation
{
    FBOM_DATA_LOCATION_NONE = 0x00,
    FBOM_DATA_LOCATION_STATIC = 0x01,
    FBOM_DATA_LOCATION_INPLACE = 0x02,
    FBOM_DATA_LOCATION_EXT_REF = 0x04
};

struct FBOMNameTable
{
    HashMap<WeakName, ANSIString>   values;

    HYP_FORCE_INLINE
    WeakName Add(const ANSIStringView &str)
    {
        const WeakName name = CreateWeakNameFromDynamicString(str);

        return Add(str, name);
    }

    HYP_FORCE_INLINE
    WeakName Add(const ANSIStringView &str, WeakName name)
    {
        values.Insert(name, str);

        return name;
    }

    void RegisterAllNamesGlobally()
    {
        for (const auto &it : values) {
            CreateNameFromDynamicString(it.second);
        }
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    UniqueID GetUniqueID() const
        { return UniqueID(GetHashCode()); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return values.GetHashCode(); }

    [[nodiscard]]
    String ToString() const
    {
        String str;

        for (const auto &pair : values) {
            str += String::ToString(pair.first.GetID()) + " : " + String(pair.second) + "\n";
        }

        return str;
    }
};

using FBOMStaticDataType = Variant<FBOMObject, FBOMType, FBOMData, FBOMNameTable>;

struct FBOMStaticData
{
    enum
    {
        FBOM_STATIC_DATA_NONE = 0x00,
        FBOM_STATIC_DATA_OBJECT = 0x01,
        FBOM_STATIC_DATA_TYPE = 0x02,
        FBOM_STATIC_DATA_DATA = 0x04,
        FBOM_STATIC_DATA_NAME_TABLE = 0x08
    } type;


    int64                           offset;
    FBOMStaticDataType              data;
    EnumFlags<FBOMStaticDataFlags>  flags;

    FBOMStaticData()
        : type(FBOM_STATIC_DATA_NONE),
          offset(-1),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMObject &object_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_OBJECT),
          data(object_data),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMType &type_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_TYPE),
          data(type_data),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMData &data_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_DATA),
          data(data_data),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(const FBOMNameTable &name_table_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_NAME_TABLE),
          data(name_table_data),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMObject &&object, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_OBJECT),
          data(std::move(object)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMType &&type, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_TYPE),
          data(std::move(type)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMData &&data, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_DATA),
          data(std::move(data)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    explicit FBOMStaticData(FBOMNameTable &&name_table, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_NAME_TABLE),
          data(std::move(name_table)),
          offset(offset),
          flags(FBOMStaticDataFlags::NONE)
    {
    }

    FBOMStaticData(const FBOMStaticData &other)
        : type(other.type),
          data(other.data),
          offset(other.offset),
          flags(other.flags),
          m_id(other.m_id)
    {
    }

    FBOMStaticData &operator=(const FBOMStaticData &other)
    {
        type = other.type;
        data = other.data;
        offset = other.offset;
        flags = other.flags;
        m_id = other.m_id;

        return *this;
    }

    FBOMStaticData(FBOMStaticData &&other) noexcept
        : type(other.type),
          data(std::move(other.data)),
          offset(other.offset),
          flags(other.flags),
          m_id(std::move(other.m_id))
    {
        other.type = FBOM_STATIC_DATA_NONE;
        other.offset = -1;
        other.flags = FBOMStaticDataFlags::NONE;
    }

    FBOMStaticData &operator=(FBOMStaticData &&other) noexcept
    {
        type = other.type;
        data = std::move(other.data);
        offset = other.offset;
        flags = other.flags;
        m_id = std::move(other.m_id);

        other.type = FBOM_STATIC_DATA_NONE;
        other.offset = -1;
        other.flags = FBOMStaticDataFlags::NONE;

        return *this;
    }

    ~FBOMStaticData() = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const FBOMStaticData &other) const
        { return offset < other.offset; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsWritten() const
        { return flags & FBOMStaticDataFlags::WRITTEN; }

    HYP_FORCE_INLINE
    void SetIsWritten(bool is_written)
    {
        if (is_written) {
            flags |= FBOMStaticDataFlags::WRITTEN;
        } else {
            flags &= ~FBOMStaticDataFlags::WRITTEN;
        }
    }

    /*! \brief Set a custom identifier for this object (overrides the underlying data's unique identifier) */
    HYP_FORCE_INLINE
    void SetUniqueID(UniqueID id)
        { m_id.Set(id); }

    HYP_FORCE_INLINE
    void UnsetCustomUniqueID()
        { m_id.Unset(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    UniqueID GetUniqueID() const
    {
        if (m_id.HasValue()) {
            return *m_id;
        }

        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return data.Get<FBOMObject>().GetUniqueID();
        case FBOM_STATIC_DATA_TYPE:
            return data.Get<FBOMType>().GetUniqueID();
        case FBOM_STATIC_DATA_DATA:
            return data.Get<FBOMData>().GetUniqueID();
        case FBOM_STATIC_DATA_NAME_TABLE:
            return data.Get<FBOMNameTable>().GetUniqueID();
        default:
            return UniqueID(0);
        }
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return data.Get<FBOMObject>().GetHashCode();
        case FBOM_STATIC_DATA_TYPE:
            return data.Get<FBOMType>().GetHashCode();
        case FBOM_STATIC_DATA_DATA:
            return data.Get<FBOMData>().GetHashCode();
        case FBOM_STATIC_DATA_NAME_TABLE:
            return data.Get<FBOMNameTable>().GetHashCode();
        default:
            return HashCode();
        }

        // return hash_code;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    String ToString() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return data.Get<FBOMObject>().ToString();
        case FBOM_STATIC_DATA_TYPE:
            return data.Get<FBOMType>().ToString();
        case FBOM_STATIC_DATA_DATA:
            return data.Get<FBOMData>().ToString();
        case FBOM_STATIC_DATA_NAME_TABLE:
            return data.Get<FBOMNameTable>().ToString();
        default:
            return "???";
        }
    }

private:
    // Optional custom set ID
    Optional<UniqueID>  m_id;
};

class HYP_API FBOM
{
public:
    static constexpr SizeType header_size = 32;
    static constexpr char header_identifier[] = { 'H', 'Y', 'P', '\0' };
    static constexpr FBOMVersion version = FBOMVersion { 1, 2, 0 };

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

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool HasCustomLoaderForType(const FBOMType &type) const
        { return FBOM::GetInstance().GetLoader(type.name) != nullptr; }

    FBOMCommand NextCommand(BufferedReader *);
    FBOMCommand PeekCommand(BufferedReader *);
    FBOMResult Eat(BufferedReader *, FBOMCommand, bool read = true);

    template <class StringType>
    FBOMResult ReadString(BufferedReader *, StringType &out_string);

    FBOMResult ReadObjectType(BufferedReader *, FBOMType &out_type);
    FBOMResult ReadNameTable(BufferedReader *, FBOMNameTable &out_name_table);
    FBOMResult ReadData(BufferedReader *, FBOMData &out_data);
    FBOMResult ReadPropertyName(BufferedReader *, Name &out_property_name);
    FBOMResult ReadObject(BufferedReader *, FBOMObject &out_object, FBOMObject *root);

    FBOMResult ReadRawData(BufferedReader *, SizeType count, ByteBuffer &out_byte_buffer);

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

HYP_DISABLE_OPTIMIZATION;
class FBOMWriter
{
public:
    FBOMWriter();
    FBOMWriter(const FBOMWriter &other)             = delete;
    FBOMWriter &operator=(const FBOMWriter &other)  = delete;
    FBOMWriter(FBOMWriter &&other) noexcept;
    FBOMWriter &operator=(FBOMWriter &&other) noexcept;

    ~FBOMWriter();

    template <class T>
    typename std::enable_if_t<!std::is_same_v<NormalizedType<T>, FBOMObject>, FBOMResult>
    Append(const T &object, FBOMObjectFlags flags = FBOM_OBJECT_FLAGS_NONE)
    {
        FBOMMarshalerBase *marshal = FBOM::GetInstance().GetLoader<NormalizedType<T>>();
        AssertThrowMsg(marshal != nullptr, "No registered marshal class for type: %s", TypeNameWithoutNamespace<NormalizedType<T>>().Data());

        FBOMObjectMarshalerBase<NormalizedType<T>> *marshal_derived = dynamic_cast<FBOMObjectMarshalerBase<NormalizedType<T>> *>(marshal);
        AssertThrowMsg(marshal_derived != nullptr, "Marshal class type mismatch for type %s", TypeNameWithoutNamespace<NormalizedType<T>>().Data());

        FBOMObject base(marshal_derived->GetObjectType());
        base.GenerateUniqueID(object, flags);

        if (FBOMResult err = marshal_derived->Serialize(object, base)) {
            m_write_stream.m_last_result = err;

            return err;
        }

        return Append(base);
    }

    FBOMResult Append(const FBOMObject &object);
    FBOMResult Emit(ByteWriter *out);

private:
    FBOMResult WriteExternalObjects();
    void BuildStaticData();
    void Prune(const FBOMObject &);

    FBOMResult WriteHeader(ByteWriter *out);
    FBOMResult WriteStaticData(ByteWriter *out);

    FBOMResult WriteObject(ByteWriter *out, const FBOMObject &object, UniqueID id);
    FBOMResult WriteObject(ByteWriter *out, const FBOMObject &object)
        { return WriteObject(out, object, object.GetUniqueID()); }

    FBOMResult WriteObjectType(ByteWriter *out, const FBOMType &type, UniqueID id);
    FBOMResult WriteObjectType(ByteWriter *out, const FBOMType &type)
        { return WriteObjectType(out, type, type.GetUniqueID()); }

    FBOMResult WriteData(ByteWriter *out, const FBOMData &data, UniqueID id);
    FBOMResult WriteData(ByteWriter *out, const FBOMData &data)
        { return WriteData(out, data, data.GetUniqueID()); }

    FBOMResult WriteNameTable(ByteWriter *out, const FBOMNameTable &name_table, UniqueID id);
    FBOMResult WriteNameTable(ByteWriter *out, const FBOMNameTable &name_table)
        { return WriteNameTable(out, name_table, name_table.GetUniqueID()); }

    FBOMResult WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &) const;

    void AddObjectData(const FBOMObject &, UniqueID id);
    
    UniqueID AddStaticData(const FBOMType &);
    UniqueID AddStaticData(const FBOMObject &);
    UniqueID AddStaticData(const FBOMData &);
    UniqueID AddStaticData(const FBOMNameTable &);

    struct WriteStream
    {
        // FBOMNameTable                       m_name_table;
        UniqueID                            m_name_table_id;
        FlatMap<String, FBOMExternalData>   m_external_objects; // map filepath -> external object
        HashMap<UniqueID, FBOMStaticData>   m_static_data;    // map hashcodes to static data to be stored.
        FlatMap<UniqueID, int>              m_hash_use_count_map;
        Array<FBOMObject>                   m_object_data;// TODO: make multiple root objects be supported by the loader
        SizeType                            m_static_data_offset = 0;
        FBOMResult                          m_last_result = FBOMResult::FBOM_OK;

        WriteStream();
        WriteStream(const WriteStream &other)                   = default;
        WriteStream &operator=(const WriteStream &other)        = default;
        WriteStream(WriteStream &&other) noexcept               = default;
        WriteStream &operator=(WriteStream &&other) noexcept    = default;
        ~WriteStream()                                          = default;

        FBOMDataLocation GetDataLocation(const UniqueID &unique_id, const FBOMStaticData **out_static_data, String &out_external_key) const;
        void MarkStaticDataWritten(const UniqueID &unique_id);

        [[nodiscard]]
        HYP_FORCE_INLINE
        FBOMNameTable &GetNameTable()
        {
            auto it = m_static_data.Find(m_name_table_id);
            AssertThrow(it != m_static_data.End());

            return it->second.data.Get<FBOMNameTable>();
        }
    } m_write_stream;

private:
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

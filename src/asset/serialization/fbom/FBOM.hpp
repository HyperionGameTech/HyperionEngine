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
    MAJOR,
    MINOR,
    PATCH,
    
    DEFAULT = uint32(MAJOR) | uint32(MINOR)
};

HYP_MAKE_ENUM_FLAGS(FBOMVersionCompareMode)

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


    int64               offset;
    FBOMStaticDataType  data;
    bool                written;

    FBOMStaticData()
        : type(FBOM_STATIC_DATA_NONE),
          offset(-1),
          written(false)
    {
    }

    explicit FBOMStaticData(const FBOMObject &object_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_OBJECT),
          data(object_data),
          offset(offset),
          written(false) {}

    explicit FBOMStaticData(const FBOMType &type_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_TYPE),
          data(type_data),
          offset(offset),
          written(false) {}

    explicit FBOMStaticData(const FBOMData &data_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_DATA),
          data(data_data),
          offset(offset),
          written(false) {}

    explicit FBOMStaticData(const FBOMNameTable &name_table_data, int64 offset = -1)
        : type(FBOM_STATIC_DATA_NAME_TABLE),
          data(name_table_data),
          offset(offset),
          written(false) {}

    explicit FBOMStaticData(FBOMObject &&object, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_OBJECT),
          data(std::move(object)),
          offset(offset),
          written(false) {}

    explicit FBOMStaticData(FBOMType &&type, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_TYPE),
          data(std::move(type)),
          offset(offset),
          written(false) {}

    explicit FBOMStaticData(FBOMData &&data, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_DATA),
          data(std::move(data)),
          offset(offset),
          written(false) {}

    explicit FBOMStaticData(FBOMNameTable &&name_table, int64 offset = -1) noexcept
        : type(FBOM_STATIC_DATA_NAME_TABLE),
          data(std::move(name_table)),
          offset(offset),
          written(false) {}

    FBOMStaticData(const FBOMStaticData &other)
        : type(other.type),
          data(other.data),
          offset(other.offset),
          written(other.written) {}

    FBOMStaticData &operator=(const FBOMStaticData &other)
    {
        type = other.type;
        data = other.data;
        offset = other.offset;
        written = other.written;

        return *this;
    }

    FBOMStaticData(FBOMStaticData &&other) noexcept
        : type(other.type),
          data(std::move(other.data)),
          offset(other.offset),
          written(other.written)
    {
        other.type = FBOM_STATIC_DATA_NONE;
        other.offset = -1;
        other.written = false;
    }

    FBOMStaticData &operator=(FBOMStaticData &&other) noexcept
    {
        type = other.type;
        data = std::move(other.data);
        offset = other.offset;
        written = other.written;

        other.type = FBOM_STATIC_DATA_NONE;
        other.offset = -1;
        other.written = false;

        return *this;
    }

    ~FBOMStaticData() = default;

    bool operator<(const FBOMStaticData &other) const
    {
        return offset < other.offset;
    }

    UniqueID GetUniqueID() const
    {
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
};

class HYP_API FBOM
{
public:
    static constexpr SizeType header_size = 32;
    static constexpr char header_identifier[] = { 'H', 'Y', 'P', '\0' };
    static constexpr FBOMVersion version = FBOMVersion { 1, 1, 0 };

    static FBOM &GetInstance();

    FBOM();
    FBOM(const FBOM &other)             = delete;
    FBOM &operator=(const FBOM &other)  = delete;
    ~FBOM();
    
    void RegisterLoader(TypeID type_id, UniquePtr<FBOMMarshalerBase> &&marshal);

    [[nodiscard]]
    FBOMMarshalerBase *GetLoader(ANSIStringView type_name) const;

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

    FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out_object)
    {
        const FBOMMarshalerBase *loader = FBOM::GetInstance().GetLoader(in.m_object_type.name);

        if (!loader) {
            return { FBOMResult::FBOM_ERR, "Loader not registered for type" };
        }

        return loader->Deserialize(in, out_object);
    }
    
    FBOMResult Deserialize(BufferedReader &reader, FBOMDeserializedObject &out_object)
    {
        FBOMObject obj;
        FBOMResult res = Deserialize(reader, obj);

        if (res.value != FBOMResult::FBOM_OK) {
            return res;
        }

        return Deserialize(obj, out_object);
    }

    FBOMResult Deserialize(BufferedReader &reader, FBOMObject &out)
    {
        if (reader.Eof()) {
            return { FBOMResult::FBOM_ERR, "Stream not open" };
        }

        FBOMObject root(FBOMObjectType("ROOT"));

        { // read header
            ubyte header_bytes[FBOM::header_size];

            if (reader.Max() < FBOM::header_size) {
                return { FBOMResult::FBOM_ERR, "Invalid header" };
            }

            reader.Read(header_bytes, FBOM::header_size);

            if (std::strncmp(reinterpret_cast<const char *>(header_bytes), FBOM::header_identifier, sizeof(FBOM::header_identifier) - 1) != 0) {
                return { FBOMResult::FBOM_ERR, "Invalid header identifier" };
            }

            // read endianness
            const ubyte endianness = header_bytes[sizeof(FBOM::header_identifier)];
            
            // set if it needs to swap endianness.
            m_swap_endianness = bool(endianness) != IsBigEndian();

            // get version info
            uint32 binary_version;
            Memory::MemCpy(&binary_version, header_bytes + sizeof(FBOM::header_identifier) + sizeof(uint8), sizeof(uint32));

            int compatibility_test_result = FBOMVersion::TestCompatibility(binary_version, FBOM::version);

            if (compatibility_test_result != 0) {
                return { FBOMResult::FBOM_ERR, "Unsupported binary version" };
            }
        }

        m_static_data_pool.Clear();
        m_in_static_data = false;

        // expect first FBOMObject defined
        FBOMCommand command = FBOM_NONE;

        while (!reader.Eof()) {
            command = PeekCommand(&reader);

            if (auto err = Handle(&reader, command, &root)) {
                return err;
            }
        }

        if (root.nodes->Empty()) {
            return { FBOMResult::FBOM_ERR, "No object added to root" };
        }

        if (root.nodes->Size() > 1) {
            return { FBOMResult::FBOM_ERR, "> 1 objects added to root (not supported in current implementation)" };
        }

        out = root.nodes->Front();

        return { FBOMResult::FBOM_OK };
    }

    
    FBOMResult LoadFromFile(const String &path, FBOMObject &out)
    {
        DebugLog(LogType::Debug, "FBOM: Loading file %s\n", path.Data());

        // Include our root dir as part of the path
        if (m_config.base_path.Empty()) {
            m_config.base_path = FileSystem::RelativePath(StringUtil::BasePath(path.Data()), FileSystem::CurrentPath()).c_str();
        }

        const FilePath read_path { FileSystem::Join(m_config.base_path.Data(), FilePath(path).Basename().Data()).c_str()};

        BufferedReader reader(RC<FileBufferedReaderSource>(new FileBufferedReaderSource(read_path)));

        return Deserialize(reader, out);
    }

    FBOMResult LoadFromFile(const String &path, FBOMDeserializedObject &out)
    {
        FBOMObject object;
        
        if (auto err = LoadFromFile(path, object)) {
            return err;
        }

        out = object.deserialized;

        return { FBOMResult::FBOM_OK };
    }

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

    bool HasCustomMarshalerForType(const FBOMType &type) const
    {
        return FBOM::GetInstance().GetLoader(type.name) != nullptr;
    }

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

        constexpr SizeType size = sizeof(NormalizedType<T>);

        ByteBuffer byte_buffer;

        if (FBOMResult err = ReadRawData(reader, size, byte_buffer)) {
            return err;
        }

        Memory::MemCpy(static_cast<void *>(out_ptr), static_cast<const void *>(byte_buffer.Data()), size);

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

class FBOMWriter
{
public:
    FBOMWriter();

    FBOMWriter(const FBOMWriter &other) = delete;
    FBOMWriter &operator=(const FBOMWriter &other) = delete;

    FBOMWriter(FBOMWriter &&other) noexcept;
    FBOMWriter &operator=(FBOMWriter &&other) noexcept;

    ~FBOMWriter();

    template <class T, class Marshal = FBOMMarshaler<T>>
    typename std::enable_if_t<!std::is_same_v<NormalizedType<T>, FBOMObject>, FBOMResult>
    Append(const T &object, FBOMObjectFlags flags = FBOM_OBJECT_FLAGS_NONE)
    {
        if (auto err = m_write_stream.m_last_result) {
            return err;
        }

        Marshal marshal;

        FBOMObject base(marshal.GetObjectType());
        base.GenerateUniqueID(object, flags);

        if (auto err = marshal.Serialize(object, base)) {
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
    FBOMResult WriteStaticDataToByteStream(ByteWriter *out);

    FBOMResult WriteObject(ByteWriter *out, const FBOMObject &);
    FBOMResult WriteObjectType(ByteWriter *out, const FBOMType &);
    FBOMResult WriteData(ByteWriter *out, const FBOMData &);
    FBOMResult WriteNameTable(ByteWriter *out, const FBOMNameTable &);
    FBOMResult WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &) const;

    void AddObjectData(const FBOMObject &);
    
    UniqueID AddStaticData(const FBOMType &);
    UniqueID AddStaticData(const FBOMObject &);
    UniqueID AddStaticData(const FBOMData &);
    UniqueID AddStaticData(const FBOMNameTable &);

    struct WriteStream
    {
        UniqueID                            m_name_table_id;
        FlatMap<String, FBOMExternalData>   m_external_objects; // map filepath -> external object
        HashMap<UniqueID, FBOMStaticData>   m_static_data;    // map hashcodes to static data to be stored.
        FlatMap<UniqueID, int>              m_hash_use_count_map;
        Array<FBOMObject>                   m_object_data;// TODO: make multiple root objects be supported by the loader
        SizeType                            m_static_data_offset = 0;
        FBOMResult                          m_last_result = FBOMResult::FBOM_OK;

        WriteStream();
        WriteStream(const WriteStream &other) = default;
        WriteStream &operator=(const WriteStream &other) = default;
        WriteStream(WriteStream &&other) noexcept = default;
        WriteStream &operator=(WriteStream &&other) noexcept = default;
        ~WriteStream() = default;

        FBOMDataLocation GetDataLocation(const UniqueID &unique_id, FBOMStaticData &out_static_data, String &out_external_key) const;
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
        { return AddStaticData(UniqueID(), std::move(static_data)); }
};

} // namespace fbom
} // namespace hyperion

#endif

#ifndef HYPERION_V2_FBOM_HPP
#define HYPERION_V2_FBOM_HPP

#include <core/lib/Optional.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/String.hpp>
#include <core/lib/Any.hpp>
#include <core/lib/Variant.hpp>
#include <core/lib/ByteBuffer.hpp>
#include <core/lib/UniqueID.hpp>
#include <core/Class.hpp>
#include <math/MathUtil.hpp>
#include <Constants.hpp>
#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>
#include <util/fs/FsUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <vector>
#include <string>
#include <map>
#include <type_traits>
#include <sstream>
#include <unordered_map>
#include <iomanip>
#include <memory>
#include <map>
#include <cstring>

using std::memset;
using std::memcpy;

#include <asset/serialization/fbom/FBOMObject.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMType.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMLoadable.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>

namespace hyperion {

namespace v2 {

class Engine;
namespace fbom {

class FBOMObjectType;

class FBOMObject;
class FBOMReader;
class FBOMWriter;


enum FBOMCommand : UInt8
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

struct FBOMStaticData
{
    enum {
        FBOM_STATIC_DATA_NONE = 0x00,
        FBOM_STATIC_DATA_OBJECT = 0x01,
        FBOM_STATIC_DATA_TYPE = 0x02,
        FBOM_STATIC_DATA_DATA = 0x04
    } type;


    Int64 offset;

    FBOMObject object_data;
    FBOMType type_data;
    Optional<FBOMData> data_data;

    bool written;

    FBOMStaticData()
        : type(FBOM_STATIC_DATA_NONE),
          offset(-1),
          written(false)
    {
    }

    explicit FBOMStaticData(const FBOMObject &object_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_OBJECT),
          object_data(object_data),
          offset(offset),
          written(false) {}
    explicit FBOMStaticData(const FBOMType &type_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_TYPE),
          type_data(type_data),
          offset(offset),
          written(false) {}
    explicit FBOMStaticData(const FBOMData &data_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_DATA),
          data_data(data_data),
          offset(offset),
          written(false) {}
    FBOMStaticData(const FBOMStaticData &other)
        : type(other.type),
          object_data(other.object_data),
          type_data(other.type_data),
          data_data(other.data_data),
          offset(other.offset),
          written(other.written) {}

    FBOMStaticData &operator=(const FBOMStaticData &other)
    {
        type = other.type;
        object_data = other.object_data;
        type_data = other.type_data;
        data_data = other.data_data;
        offset = other.offset;
        written = other.written;

        return *this;
    }

    FBOMStaticData(FBOMStaticData &&other) noexcept
        : type(other.type),
          object_data(std::move(other.object_data)),
          type_data(std::move(other.type_data)),
          data_data(std::move(other.data_data)),
          offset(other.offset),
          written(other.written)
    {
        other.type = FBOM_STATIC_DATA_NONE;
        other.offset = 0;
        other.written = false;
    }

    FBOMStaticData &operator=(FBOMStaticData &&other) noexcept
    {
        type = other.type;
        object_data = std::move(other.object_data);
        type_data = std::move(other.type_data);
        data_data = std::move(other.data_data);
        offset = other.offset;
        written = other.written;

        other.type = FBOM_STATIC_DATA_NONE;
        other.offset = 0;
        other.written = false;

        return *this;
    }

    ~FBOMStaticData() = default;

    // bool operator==(const FBOMStaticData &other) const
    // {
    //     if (
    //         type != other.type
    //         || offset != other.offset
    //         || written != other.written
    //     )
    //     {
    //         return false;
    //     }

    //     switch (type) {
    //     case FBOM_STATIC_DATA_OBJECT:
    //         return object_data == other.object_data;
    //     case FBOM_STATIC_DATA_TYPE:
    //         return type_data == other.type_data;
    //     case FBOM_STATIC_DATA_DATA:
    //         return data_data.Get() == other.data_data.Get();
    //     case FBOM_STATIC_DATA_NONE:
    //         return true;
    //     }
    // }

    bool operator<(const FBOMStaticData &other) const
    {
        return offset < other.offset;
    }

    UniqueID GetUniqueID() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return object_data.GetUniqueID();
        case FBOM_STATIC_DATA_TYPE:
            return type_data.GetUniqueID();
        case FBOM_STATIC_DATA_DATA:
            return data_data.Get().GetUniqueID();
        default:
            return UniqueID();
        }
    }

    HashCode GetHashCode() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return object_data.GetHashCode();
        case FBOM_STATIC_DATA_TYPE:
            return type_data.GetHashCode();
        case FBOM_STATIC_DATA_DATA:
            return data_data.Get().GetHashCode();
        default:
            return HashCode();
        }

        // return hash_code;
    }

    std::string ToString() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return object_data.ToString();
        case FBOM_STATIC_DATA_TYPE:
            return type_data.ToString();
        case FBOM_STATIC_DATA_DATA:
            return data_data.Get().ToString();
        default:
            return "???";
        }
    }
};



class FBOM {
    FlatMap<String, std::unique_ptr<FBOMMarshalerBase>> marshalers;

public:
    static constexpr SizeType header_size = 32;
    static constexpr char header_identifier[] = { 'H', 'Y', 'P', '\0' };
    static constexpr UInt32 version = 0x1;

    FBOM();
    FBOM(const FBOM &other) = delete;
    FBOM &operator=(const FBOM &other) = delete;
    ~FBOM();

    template <class T, class Marshaler = FBOMMarshaler<T>>
    void RegisterLoader()
    {
        static_assert(std::is_base_of_v<FBOMMarshalerBase, Marshaler>,
            "Marshaler class must be a derived class of FBOMMarshalBase");

        auto loader = std::make_unique<Marshaler>();
        const auto name = loader->GetObjectType().name;

        marshalers.Set(name, std::move(loader));
    }

    FBOMMarshalerBase *GetLoader(const String &type_name)
    {
        auto it = marshalers.Find(type_name);

        AssertThrowMsg(
            it != marshalers.End(),
            "%s is not a registered type!",
            type_name.Data()
        );

        return it->second.get();
    }

    static FBOM &GetInstance()
    {
        static FBOM instance;

        return instance;
    }
};

struct FBOMConfig
{
    bool continue_on_external_load_error = true;

    String base_path;
    FlatMap<Pair<String, UInt32>, FBOMObject> external_data_cache;
};

class FBOMReader
{
public:
    FBOMReader(const FBOMConfig &config);
    ~FBOMReader();

    FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out_object)
    {
        const String object_type_name(in.m_object_type.name);
        const auto *loader = FBOM::GetInstance().GetLoader(object_type_name);

        if (!loader) {
            return { FBOMResult::FBOM_ERR, "Loader not registered for type" };
        }

        return loader->Deserialize(in, out_object);
    }

    FBOMResult LoadFromFile(const String &path, FBOMObject &out)
    {
        FBOMObject root(FBOMObjectType("ROOT"));

        // Include our root dir as part of the path
        const auto current_dir = FileSystem::CurrentPath();
        const auto base_path = StringUtil::BasePath(path.Data());
        root.SetProperty("base_path", FBOMString(), base_path.size(), base_path.data());

        FileByteReader reader(FileSystem::Join(base_path, std::string(FilePath(path).Basename().Data())));

        if (reader.Eof()) {
            return { FBOMResult::FBOM_ERR, "File not open" };
        }

        { // read header
            UByte header_bytes[FBOM::header_size];

            if (reader.Max() < FBOM::header_size) {
                return { FBOMResult::FBOM_ERR, "Invalid header" };
            }

            reader.Read(header_bytes, FBOM::header_size);

            if (std::strncmp(reinterpret_cast<const char *>(header_bytes), FBOM::header_identifier, sizeof(FBOM::header_identifier) - 1) != 0) {
                return { FBOMResult::FBOM_ERR, "Invalid header identifier" };
            }

            // read endianness
            const UByte endianness = header_bytes[sizeof(FBOM::header_identifier)];
            
            // set if it needs to swap endianness.
            m_swap_endianness = bool(endianness) != IsBigEndian();

            // get version info
            UInt32 binary_version;
            Memory::Copy(&binary_version, header_bytes + sizeof(FBOM::header_identifier) + sizeof(UInt8), sizeof(UInt32));

            if (binary_version > FBOM::version || binary_version < 0x1) {
                return { FBOMResult::FBOM_ERR, "Invalid binary version specified in header" };
            }
        }

        m_static_data_pool.clear();
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
        if constexpr (sizeof(T) == 1) {
            return;
        } else if (m_swap_endianness) {
            SwapEndianness(value);
        }
    }

    FBOMCommand NextCommand(ByteReader *);
    FBOMCommand PeekCommand(ByteReader *);
    FBOMResult Eat(ByteReader *, FBOMCommand, bool read = true);

    String ReadString(ByteReader *);
    FBOMType ReadObjectType(ByteReader *);
    FBOMResult ReadData(ByteReader *, FBOMData &data);
    FBOMResult ReadObject(ByteReader *, FBOMObject &object, FBOMObject *root);

    FBOMResult Handle(ByteReader *, FBOMCommand, FBOMObject *root);

    FBOMConfig m_config;

    std::vector<FBOMType> m_registered_types;

    bool m_in_static_data;
    std::vector<FBOMStaticData> m_static_data_pool;

    bool m_swap_endianness;
};

struct FBOMExternalData
{
    FlatMap<UniqueID, FBOMObject> objects;
};

class FBOMWriter
{
public:
    FBOMWriter();
    ~FBOMWriter();

    template <class T, class Marshaler = FBOMMarshaler<T>>
    typename std::enable_if_t<!std::is_same_v<NormalizedType<T>, FBOMObject>, FBOMResult>
    Append(const T &object)
    {
        if (auto err = m_write_stream.m_last_result) {
            return err;
        }

        Marshaler marshal;

        FBOMObject base(marshal.GetObjectType());

        // Set the ID of the object so we can reuse it.
        // TODO: clean this up a bit.
        if constexpr (std::is_base_of_v<EngineComponentBaseBase, T>) {
            base.m_unique_id = UniqueID(object.GetID());
        } else {
            base.m_unique_id = UniqueID(object);
        }

        if (auto err = marshal.Serialize(object, base)) {
            m_write_stream.m_last_result = err;

            return err;
        }

        return Append(std::move(base));
    }

    FBOMResult Append(const FBOMObject &object);
    FBOMResult Emit(ByteWriter *out);

private:
    FBOMResult WriteExternalObjects();
    void BuildStaticData();
    void Prune(FBOMObject &);

    FBOMResult WriteHeader(ByteWriter *out);
    FBOMResult WriteStaticDataToByteStream(ByteWriter *out);

    FBOMResult WriteObject(ByteWriter *out, const FBOMObject &);
    FBOMResult WriteObjectType(ByteWriter *out, const FBOMType &);
    FBOMResult WriteData(ByteWriter *out, const FBOMData &);
    FBOMResult WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &) const;

    void AddObjectData(const FBOMObject &);
    void AddStaticData(const FBOMType &);
    void AddStaticData(const FBOMObject &);
    void AddStaticData(const FBOMData &);

    struct WriteStream
    {
        FlatMap<String, FBOMExternalData> m_external_objects; // map filepath -> external object
        std::unordered_map<UniqueID, FBOMStaticData> m_static_data; // map hashcodes to static data to be stored.
        FlatMap<UniqueID, int> m_hash_use_count_map;
        std::vector<FBOMObject> m_object_data; // TODO: make multiple objects be supported by the loader.
        SizeType m_static_data_offset = 0;
        FBOMResult m_last_result = FBOMResult::FBOM_OK;

        FBOMDataLocation GetDataLocation(const UniqueID &unique_id, FBOMStaticData &out) const;
        void MarkStaticDataWritten(const UniqueID &unique_id);
    } m_write_stream;
};

} // namespace fbom
} // namespace v2
} // namespace hyperion

#endif

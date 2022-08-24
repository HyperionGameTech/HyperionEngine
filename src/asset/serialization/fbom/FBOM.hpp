#ifndef HYPERION_V2_FBOM_HPP
#define HYPERION_V2_FBOM_HPP

#include <core/lib/Optional.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/String.hpp>
#include <core/lib/Any.hpp>
#include <core/lib/Variant.hpp>
#include <core/Class.hpp>
#include <HashCode.hpp>
#include <math/MathUtil.hpp>
#include <Constants.hpp>
#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>
#include <util/fs/FsUtil.hpp>

#include <Constants.hpp>
#include <Types.hpp>

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
class FBOMLoader;
class FBOMWriter;


enum FBOMCommand : UInt8 {
    FBOM_NONE = 0,
    FBOM_OBJECT_START,
    FBOM_OBJECT_END,
    FBOM_STATIC_DATA_START,
    FBOM_STATIC_DATA_END,
    FBOM_DEFINE_PROPERTY,
};

enum FBOMDataLocation {
    FBOM_DATA_LOCATION_NONE = 0x00,
    FBOM_DATA_LOCATION_STATIC = 0x01,
    FBOM_DATA_LOCATION_INPLACE = 0x02,
    FBOM_DATA_LOCATION_EXT_REF = 0x04
};

struct FBOMStaticData {
    enum {
        FBOM_STATIC_DATA_NONE = 0x00,
        FBOM_STATIC_DATA_OBJECT = 0x01,
        FBOM_STATIC_DATA_TYPE = 0x02,
        FBOM_STATIC_DATA_DATA = 0x04
    } type;

    int64_t offset;

    // Variant<void *, FBOMObject, FBOMType, FBOMData> data;
    // void *raw_data;
    FBOMObject object_data;
    FBOMType type_data;
    Optional<FBOMData> data_data;
    bool written;

    FBOMStaticData()
        : type(FBOM_STATIC_DATA_NONE),
        //   raw_data(nullptr),
          offset(-1),
          written(false)
    {
    }

    explicit FBOMStaticData(const FBOMObject &object_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_OBJECT),
          object_data(object_data),
        //   raw_data(nullptr),
          offset(offset),
          written(false) {}
    explicit FBOMStaticData(const FBOMType &type_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_TYPE),
          type_data(type_data),
        //   raw_data(nullptr),
          offset(offset),
          written(false) {}
    explicit FBOMStaticData(const FBOMData &data_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_DATA),
          data_data(data_data),
        //   raw_data(nullptr),
          offset(offset),
          written(false) {}
    FBOMStaticData(const FBOMStaticData &other)
        : type(other.type),
          object_data(other.object_data),
          type_data(other.type_data),
          data_data(other.data_data),
        //   raw_data(other.raw_data),
          offset(other.offset),
          written(other.written) {}

    // FBOMStaticData(const FBOMStaticData &other) = delete;

    FBOMStaticData &operator=(const FBOMStaticData &other)
    {
        type = other.type;
        object_data = other.object_data;
        type_data = other.type_data;
        data_data = other.data_data;
        // raw_data = other.raw_data;
        offset = other.offset;
        written = other.written;

        return *this;
    }

    // FBOMStaticData &operator=(const FBOMStaticData &other) = delete;

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

    inline HashCode GetHashCode() const
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
    }

    inline std::string ToString() const
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

        AssertThrowMsg(it != marshalers.End(), "Not a registered type!");

        return it->second.get();
    }

    static FBOM &GetInstance()
    {
        static FBOM instance;

        return instance;
    }
};

class FBOMLoader {
public:
    FBOMLoader(Engine *engine);
    ~FBOMLoader();

    FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out_object)
    {
        const String object_type_name(in.m_object_type.name);
        const auto *loader = FBOM::GetInstance().GetLoader(object_type_name);

        if (!loader) {
            return FBOMResult(FBOMResult::FBOM_ERR, "Loader not registered for type");
        }

        return loader->Deserialize(m_engine, in, out_object);
    }

    FBOMResult LoadFromFile(const String &path, FBOMObject &out)
    {
        const auto base_path = StringUtil::BasePath(path.Data());

        FBOMObject root(FBOMObjectType("ROOT"));
        root.SetProperty("base_path", FBOMString(), base_path.size(), base_path.data());

        // Include our root dir as part of the path
        std::string root_dir = FileSystem::CurrentPath();
        FileByteReader reader(FileSystem::Join(root_dir, std::string(path.Data())));

        if (reader.Eof()) {
            return { FBOMResult::FBOM_ERR, "File not open" };
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
    FBOMCommand NextCommand(ByteReader *);
    FBOMCommand PeekCommand(ByteReader *);
    FBOMResult Eat(ByteReader *, FBOMCommand, bool read = true);

    String ReadString(ByteReader *);
    FBOMType ReadObjectType(ByteReader *);
    FBOMResult ReadData(ByteReader *, FBOMData &data);
    FBOMResult ReadObject(ByteReader *, FBOMObject &object, FBOMObject *parent);

    FBOMResult Handle(ByteReader *, FBOMCommand, FBOMObject *parent);

    Engine *m_engine;

    std::vector<FBOMType> m_registered_types;

    bool m_in_static_data;
    std::vector<FBOMStaticData> m_static_data_pool;
};

struct FBOMExternalData {
    FlatMap<HashCode::ValueType, FBOMObject> objects;
};

class FBOMWriter {
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
    FBOMResult WriteStaticDataToByteStream(ByteWriter *out);

    FBOMResult WriteObject(ByteWriter *out, const FBOMObject &);
    FBOMResult WriteObjectType(ByteWriter *out, const FBOMType &);
    FBOMResult WriteData(ByteWriter *out, const FBOMData &);
    FBOMResult WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &) const;

    void AddObjectData(const FBOMObject &);
    void AddStaticData(const FBOMType &);
    void AddStaticData(const FBOMObject &);
    void AddStaticData(const FBOMData &);

    struct WriteStream {
        FlatMap<String, FBOMExternalData> m_external_objects; // map filepath -> external object
        std::map<HashCode::ValueType, FBOMStaticData> m_static_data; // map hashcodes to static data to be stored.
        std::unordered_map<HashCode::ValueType, int> m_hash_use_count_map;
        std::vector<FBOMObject> m_object_data; // TODO: make multiple objects be supported by the loader.
        size_t m_static_data_offset = 0;
        FBOMResult m_last_result = FBOMResult::FBOM_OK;

        FBOMDataLocation GetDataLocation(HashCode::ValueType, FBOMStaticData &out) const;
        void MarkStaticDataWritten(HashCode::ValueType);
    } m_write_stream;
};

} // namespace fbom
} // namespace v2
} // namespace hyperion

#endif

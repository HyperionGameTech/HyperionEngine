#ifndef FBOM_H
#define FBOM_H

#include "../loadable.h"
#include "../asset_loader.h"
#include "../../hash_code.h"
#include "../../math/math_util.h"

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


#include "result.h"
#include "type.h"
#include "base_types.h"
#include "data.h"
#include "object.h"
#include "loadable.h"

namespace hyperion {
class ByteReader;
class ByteWriter;
namespace fbom {
class FBOMObjectType;

class FBOMObject;
class FBOMLoader;
class FBOMWriter;


enum FBOMCommand {
    FBOM_NONE = 0,
    FBOM_OBJECT_START,
    FBOM_OBJECT_END,
    FBOM_STATIC_DATA_START,
    FBOM_STATIC_DATA_END,
    FBOM_DEFINE_PROPERTY,
};

using FBOMDeserializeFunction = std::function<FBOMResult(FBOMLoader *, FBOMObject *, FBOMDeserialized &)>;
using FBOMSerializeFunction = std::function<FBOMResult(const FBOMWriter *, FBOMLoadable *, FBOMObject *)>;

struct FBOMMarshal {
    FBOMMarshal(FBOMDeserializeFunction deserializer, FBOMSerializeFunction serializer)
        : m_deserializer(deserializer),
          m_serializer(serializer)
    {
    }

    FBOMDeserializeFunction m_deserializer;
    FBOMSerializeFunction m_serializer;
};

#define FBOM_DEF_DESERIALIZER(l, in, out) \
    static inline fbom::FBOMResult FBOM_Deserialize(fbom::FBOMLoader *l, fbom::FBOMObject *in, fbom::FBOMDeserialized &out)
#define FBOM_DEF_SERIALIZER(w, in, out) \
    static inline fbom::FBOMResult FBOM_Serialize(const fbom::FBOMWriter *w, fbom::FBOMLoadable *in, fbom::FBOMObject *out)
#define FBOM_GET_DESERIALIZER(kls) \
    &kls::FBOM_Deserialize
#define FBOM_GET_SERIALIZER(kls) \
    &kls::FBOM_Serialize

#define FBOM_MARSHAL_CLASS(kls) \
    FBOMMarshal(FBOM_GET_DESERIALIZER(kls), FBOM_GET_SERIALIZER(kls))


enum FBOMDataLocation {
    FBOM_DATA_LOCATION_NONE = 0x00,
    FBOM_DATA_LOCATION_STATIC = 0x01,
    FBOM_DATA_LOCATION_INPLACE = 0x02
};

struct FBOMStaticData {
    enum {
        FBOM_STATIC_DATA_NONE = 0x00,
        FBOM_STATIC_DATA_OBJECT = 0x01,
        FBOM_STATIC_DATA_TYPE = 0x02,
        FBOM_STATIC_DATA_DATA = 0x04
    } type;

    int64_t offset;

    // no longer a union due to destructors needed
    void *raw_data;
    FBOMObject object_data;
    FBOMType type_data;
    std::shared_ptr<FBOMData> data_data;
    bool written;

    FBOMStaticData()
        : type(FBOM_STATIC_DATA_NONE), raw_data(nullptr), offset(-1), written(false) {}
    FBOMStaticData(const FBOMObject &object_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_OBJECT),
          object_data(object_data),
          raw_data(nullptr),
          offset(offset),
          written(false) {}
    FBOMStaticData(const FBOMType &type_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_TYPE),
          type_data(type_data),
          raw_data(nullptr),
          offset(offset),
          written(false) {}
    FBOMStaticData(const std::shared_ptr<FBOMData> &data_data, int64_t offset = -1)
        : type(FBOM_STATIC_DATA_DATA),
          data_data(data_data),
          raw_data(nullptr),
          offset(offset),
          written(false) {}
    FBOMStaticData(const FBOMStaticData &other)
        : type(other.type),
          object_data(other.object_data),
          type_data(other.type_data),
          data_data(other.data_data),
          raw_data(other.raw_data),
          offset(other.offset),
          written(other.written) {}
    FBOMStaticData &operator=(const FBOMStaticData &other)
    {
        type = other.type;
        object_data = other.object_data;
        type_data = other.type_data;
        data_data = other.data_data;
        raw_data = other.raw_data;
        offset = other.offset;
        written = other.written;

        return *this;
    }

    ~FBOMStaticData() = default;

    inline HashCode GetHashCode() const
    {
        switch (type) {
        case FBOM_STATIC_DATA_OBJECT:
            return object_data.GetHashCode();
        case FBOM_STATIC_DATA_TYPE:
            return type_data.GetHashCode();
        case FBOM_STATIC_DATA_DATA:
            return data_data->GetHashCode();
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
            return data_data->ToString();
        default:
            return "???";
        }
    }
};

class FBOMLoader : public AssetLoader {
public:
    static const std::map<std::string, FBOMMarshal> loaders; // loader type -> loader

    FBOMLoader();
    virtual ~FBOMLoader() override;

    FBOMResult Deserialize(FBOMObject *in, FBOMDeserialized &out);

    virtual std::shared_ptr<Loadable> LoadFromFile(const std::string &) override;

private:
    FBOMCommand NextCommand(ByteReader *);
    FBOMCommand PeekCommand(ByteReader *);
    FBOMResult Eat(ByteReader *, FBOMCommand, bool read = true);

    std::string ReadString(ByteReader *);
    FBOMType ReadObjectType(ByteReader *);
    FBOMResult ReadData(ByteReader *, std::shared_ptr<FBOMData> &data);
    FBOMResult ReadObject(ByteReader *, FBOMObject &object);

    FBOMResult Handle(ByteReader *, FBOMCommand, FBOMObject *parent);

    FBOMObject *root;
    FBOMObject *last;

    std::vector<FBOMType> m_registered_types;

    bool m_in_static_data;
    std::vector<FBOMStaticData> m_static_data_pool;
};

class FBOMWriter {
public:
    FBOMWriter();
    ~FBOMWriter();

    FBOMResult Serialize(FBOMLoadable *in, FBOMObject *out) const;

    FBOMResult Append(FBOMLoadable *);
    FBOMResult Append(FBOMObject);
    FBOMResult Emit(ByteWriter *out);

private:
    void BuildStaticData();
    void Prune(FBOMObject *);
    FBOMResult WriteStaticDataToByteStream(ByteWriter *out);

    FBOMResult WriteObject(ByteWriter *out, const FBOMObject &);
    FBOMResult WriteObject(ByteWriter *out, FBOMLoadable *);
    FBOMResult WriteObjectType(ByteWriter *out, const FBOMType &);
    FBOMResult WriteData(ByteWriter *out, const std::shared_ptr<FBOMData> &);
    FBOMResult WriteStaticDataUsage(ByteWriter *out, const FBOMStaticData &) const;

    void AddObjectData(const FBOMObject &);
    FBOMStaticData AddStaticData(const FBOMType &);
    FBOMStaticData AddStaticData(const FBOMObject &);
    FBOMStaticData AddStaticData(const std::shared_ptr<FBOMData> &);

    struct WriteStream {
        std::map<HashCode::Value_t, FBOMStaticData> m_static_data; // map hashcodes to static data to be stored.
        std::unordered_map<HashCode::Value_t, int> m_hash_use_count_map;
        std::vector<FBOMObject> m_object_data; // TODO: make multiple objects be supported by the loader.
        size_t m_static_data_offset = 0;
        FBOMResult m_last_result = FBOMResult::FBOM_OK;

        FBOMDataLocation GetDataLocation(HashCode::Value_t, FBOMStaticData &out) const;
        void MarkStaticDataWritten(HashCode::Value_t);
    } m_write_stream;
};

} // namespace fbom
} // namespace hyperion

#endif

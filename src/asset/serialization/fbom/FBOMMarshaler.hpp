#ifndef HYPERION_V2_FBOM_MARSHALER_HPP
#define HYPERION_V2_FBOM_MARSHALER_HPP

#include <asset/serialization/fbom/FBOMDeserializedObject.hpp>
#include <asset/serialization/fbom/FBOMType.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>

#include <Constants.hpp>

namespace hyperion::v2 {
class Engine;
} // namespace hyperion::v2

namespace hyperion::v2::fbom {

class FBOMObject;

class FBOMMarshalerBase {
    friend class FBOMLoader;

public:
    virtual ~FBOMMarshalerBase() = default;

    virtual FBOMType GetObjectType() const = 0;

protected:
    virtual FBOMResult Serialize(const FBOMDeserializedObject &in, FBOMObject &out) const = 0;
    virtual FBOMResult Deserialize(Engine *, const FBOMObject &in, FBOMDeserializedObject &out) const = 0;
};

template <class T>
class FBOMObjectMarshalerBase : public FBOMMarshalerBase {
public:
    virtual ~FBOMObjectMarshalerBase() = default;

    virtual FBOMType GetObjectType() const override = 0;

    virtual FBOMResult Serialize(const T &in_object, FBOMObject &out) const = 0;
    virtual FBOMResult Deserialize(Engine *, const FBOMObject &in, T *&out_object) const = 0;

private:
    virtual FBOMResult Serialize(const FBOMDeserializedObject &in, FBOMObject &out) const override
    {
        return Serialize(in.Get<T>(), out);
    }

    virtual FBOMResult Deserialize(Engine *engine, const FBOMObject &in, FBOMDeserializedObject &out) const override
    {
        T *ptr = nullptr;

        auto result = Deserialize(engine, in, ptr);

        if (result.value != FBOMResult::FBOM_ERR) {
            out.Reset(ptr); // takes ownership of ptr
        } else {
            // we are responsible for managing the memory,
            // and we can't add the resulting ptr to the Any
            // if an error has occured, so just delete it.
            delete ptr;
        }

        return result;
    }
};

template <class T>
class FBOMMarshaler : public FBOMObjectMarshalerBase<T> {
    static_assert(resolution_failure<T>, "No marshal class defined");
};

} // namespace hyperion::v2::fbom

#endif
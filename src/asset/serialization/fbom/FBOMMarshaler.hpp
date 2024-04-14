#ifndef HYPERION_V2_FBOM_MARSHALER_HPP
#define HYPERION_V2_FBOM_MARSHALER_HPP

#include <core/Core.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/Util.hpp>

#include <asset/serialization/fbom/FBOMDeserializedObject.hpp>
#include <asset/serialization/fbom/FBOMType.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>

#include <Constants.hpp>

namespace hyperion::v2::fbom {

class FBOMObject;

class FBOMMarshalerBase
{
    friend class FBOMReader;

public:
    virtual ~FBOMMarshalerBase() = default;

    virtual FBOMType GetObjectType() const = 0;

protected:
    // virtual FBOMResult Serialize(const FBOMDeserializedObject &in, FBOMObject &out) const = 0;
    virtual FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out) const = 0;
};

template <class T>
class FBOMObjectMarshalerBase : public FBOMMarshalerBase
{
public:
    virtual ~FBOMObjectMarshalerBase() = default;

    virtual FBOMType GetObjectType() const override
        { return FBOMObjectType(TypeNameWithoutNamespace<T>().Data()); }

    virtual FBOMResult Serialize(const T &in_object, FBOMObject &out) const = 0;
    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const = 0;

private:
    // virtual FBOMResult Serialize(const FBOMDeserializedObject &in, FBOMObject &out) const override
    // {
    //     auto extracted_value = in.Get<T>();

    //     return Serialize(*extracted_value.Get(), out);
    // }

    virtual FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out) const override final
    {
        UniquePtr<void> ptr;

        auto result = Deserialize(in, ptr);

        if (result.value == FBOMResult::FBOM_OK) {
            AssertThrow(ptr != nullptr);

            auto result_value = AssetLoaderWrapper<T>::MakeResultType(std::move(ptr));
            AssertThrow(result_value.Get() != nullptr);

            out.Set<T>(std::move(result_value));
        }

        return result;
    }
};

template <class T>
class FBOMMarshaler : public FBOMObjectMarshalerBase<T>
{
    static_assert(resolution_failure<T>, "No marshal class defined");
};

} // namespace hyperion::v2::fbom

#endif
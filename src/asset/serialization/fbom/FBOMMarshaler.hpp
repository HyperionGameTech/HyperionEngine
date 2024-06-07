/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_MARSHALER_HPP
#define HYPERION_FBOM_MARSHALER_HPP

#include <core/Core.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/Util.hpp>

#include <asset/serialization/fbom/FBOMDeserializedObject.hpp>
#include <asset/serialization/fbom/FBOMType.hpp>
#include <asset/serialization/fbom/FBOMResult.hpp>

#include <Constants.hpp>

namespace hyperion::fbom {

class FBOM;
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
class FBOMObjectMarshalerBase;

template <class T>
class FBOMMarshaler;

namespace detail {

struct FBOMMarshalerRegistrationBase
{
protected:
    FBOMMarshalerRegistrationBase(TypeID type_id, UniquePtr<FBOMMarshalerBase> &&marshal);
};

template <class T, class MarshalerType>
struct FBOMMarshalerRegistration : FBOMMarshalerRegistrationBase
{
    FBOMMarshalerRegistration()
        : FBOMMarshalerRegistrationBase(TypeID::ForType<T>(), UniquePtr<FBOMMarshalerBase>(new MarshalerType()))
    {
    }
};

} // namespace detail

template <class T>
class FBOMObjectMarshalerBase : public FBOMMarshalerBase
{
public:
    virtual ~FBOMObjectMarshalerBase() = default;

    virtual FBOMType GetObjectType() const override
        { return FBOMObjectType(TypeNameWithoutNamespace<T>().Data()); }

    virtual FBOMResult Serialize(const T &in_object, FBOMObject &out) const = 0;
    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const = 0;

private:
    virtual FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out) const override final
    {
        Any any_value;

        auto result = Deserialize(in, any_value);

        if (result.value == FBOMResult::FBOM_OK) {
            AssertThrow(any_value.HasValue());
            
            out.m_value = std::move(any_value);

            /*// @TODO Come back to this
            UniquePtr<AssetLoaderWrapper<T>::ResultType> result_ptr = ptr.Cast<AssetLoaderWrapper<T>::ResultType>();
            AssertThrow(result_ptr != nullptr);

            out.Set<T>(std::move(*result_ptr));*/
        }

        return result;
    }
};

template <class T>
class FBOMMarshaler : public FBOMObjectMarshalerBase<T>
{
public:
    virtual FBOMResult Serialize(const T &in_object, FBOMObject &out) const override;
    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override;
};

#define HYP_DEFINE_MARSHAL(T, MarshalType) \
    static ::hyperion::fbom::detail::FBOMMarshalerRegistration<T, MarshalType> T##_Marshal { }


} // namespace hyperion::fbom

#endif
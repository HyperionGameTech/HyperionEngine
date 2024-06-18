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
public:
    virtual ~FBOMMarshalerBase() = default;

    virtual FBOMType GetObjectType() const = 0;
    virtual TypeID GetTypeID() const = 0;

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out) const = 0;

private:
    HYP_DEPRECATED
    FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out) const;
};

template <class T>
class FBOMObjectMarshalerBase;

template <class T>
class FBOMMarshaler;

template <class T, class MarshalerType>
struct FBOMHasMarshal
{
    static constexpr bool value = false;
};

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

    virtual TypeID GetTypeID() const override final
        { return TypeID::ForType<T>(); }

    virtual FBOMResult Serialize(const T &in, FBOMObject &out) const = 0;

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out) const override = 0;
};

template <class T>
class FBOMMarshaler : public FBOMObjectMarshalerBase<T>
{
public:
    virtual FBOMResult Serialize(const T &in_object, FBOMObject &out) const override;
    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out) const override;
};

#define HYP_DEFINE_MARSHAL(T, MarshalType) \
    template <> \
    struct FBOMHasMarshal<T, MarshalType> \
    { \
        static constexpr bool value = true; \
    }; \
    static ::hyperion::fbom::detail::FBOMMarshalerRegistration<T, MarshalType> T##_Marshal { }

#define HYP_HAS_MARSHAL(T, MarshalType) \
    (::hyperion::fbom::FBOMHasMarshal<T, MarshalType>::value)

} // namespace hyperion::fbom

#endif
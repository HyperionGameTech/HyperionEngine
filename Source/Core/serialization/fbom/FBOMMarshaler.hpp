/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/UniquePtr.hpp>
#include <Core/memory/Any.hpp>
#include <Core/memory/AnyRef.hpp>

#include <Core/Util.hpp>

#include <Core/serialization/fbom/FBOMType.hpp>
#include <Core/serialization/fbom/FBOMBaseTypes.hpp>
#include <Core/serialization/fbom/FBOMResult.hpp>

#include <Core/Constants.hpp>

namespace Hyperion {
struct BoxedValue;
class Class;
} // namespace Hyperion

namespace Hyperion::serialization {

class FBOM;
class FBOMObject;
class FBOMLoadContext;

class FBOMMarshalerBase
{
public:
    virtual ~FBOMMarshalerBase() = default;

    virtual FBOMType GetObjectType() const = 0;
    virtual TypeId GetTypeId() const = 0;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const = 0;
    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const = 0;
};

template <class T>
class FBOMMarshaler;

template <class T, class T2 = void>
class FBOMObjectMarshalerBase;

struct FBOMMarshalerRegistrationBase
{
protected:
    FBOMMarshalerRegistrationBase(TypeId typeId, ANSIStringView name, UniquePtr<FBOMMarshalerBase>&& marshal);
};

template <class T, class MarshalType>
struct FBOMMarshalerRegistration : FBOMMarshalerRegistrationBase
{
    FBOMMarshalerRegistration()
        : FBOMMarshalerRegistrationBase(TypeId::ForType<T>(), TypeNameHelper<T, true>::value.Data(), MakeUnique<MarshalType>())
    {
    }
};

template <class T>
class FBOMObjectMarshalerBase<T, std::enable_if_t<IsObjectV<T>>> : public FBOMMarshalerBase
{
public:
    using ClassType = typename T::ClassInfo::Type;

    virtual ~FBOMObjectMarshalerBase() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(TypeNameHelper<ClassType, true>::value.Data());
    }

    virtual TypeId GetTypeId() const override final
    {
        return TypeId::ForType<ClassType>();
    }

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override final
    {
        HYP_CORE_ASSERT(in.Is<ClassType>(), "Cannot serialize - given object is not of expected type");

        if (!in.Is<ClassType>())
        {
            return { FBOMResult::FBOM_ERR, "Cannot serialize - given object is not of expected type" };
        }

        return Serialize(in.Get<ClassType>(), out);
    }

    virtual FBOMResult Serialize(const ClassType& in, FBOMObject& out) const = 0;
    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, BoxedValue& out) const override = 0;
};

#define HYP_DEFINE_MARSHAL(T, MarshalType)                                                                              \
    HYP_EXPORT ::Hyperion::FBOMMarshalerRegistration<typename T::ClassInfo::Type, MarshalType> g_marshalRegistration##T \
    {                                                                                                                   \
    }

} // namespace Hyperion::serialization

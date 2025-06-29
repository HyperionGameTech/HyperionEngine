/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_HYP_CLASS_INSTANCE_MARSHAL_HPP
#define HYPERION_FBOM_HYP_CLASS_INSTANCE_MARSHAL_HPP

#include <core/Util.hpp>

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>

#include <Constants.hpp>

namespace hyperion {

template <class T>
class HypClassInstance;

// Leave implementation empty - stub class
template <>
class HypClassInstance<void>;

using HypClassInstanceStub = HypClassInstance<void>;

} // namespace hyperion

namespace hyperion::serialization {

class HYP_API HypClassInstanceMarshal : public FBOMMarshalerBase
{
public:
    virtual ~HypClassInstanceMarshal() override = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(TypeNameWithoutNamespace<HypClassInstanceStub>().Data());
    }

    virtual TypeId GetTypeId() const override final
    {
        return TypeId::ForType<HypClassInstanceStub>();
    }

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override;
    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override;

protected:
    /*! \brief Deserialize into an existing object.
     *
     *  \param in The FBOMObject to deserialize.
     *  \param hyp_class The HypClass of the instance.
     *  \param ref The instance to deserialize into.
     *  \return The result of the deserialization.
     */
    virtual FBOMResult Deserialize_Internal(FBOMLoadContext& context, const FBOMObject& in, const HypClass* hyp_class, AnyRef ref) const;
};

} // namespace hyperion::serialization

#endif
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_HYP_CLASS_INSTANCE_MARSHAL_HPP
#define HYPERION_FBOM_HYP_CLASS_INSTANCE_MARSHAL_HPP

#include <core/Util.hpp>

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>

#include <Constants.hpp>

namespace hyperion {

template <class T>
class HypClassInstance;

// Leave implementation empty - stub class
template <>
class HypClassInstance<void>;

using HypClassInstanceStub = HypClassInstance<void>;

} // namespace hyperion

namespace hyperion::fbom {

class HYP_API HypClassInstanceMarshal : public FBOMMarshalerBase
{
public:
    virtual ~HypClassInstanceMarshal() override = default;

    virtual FBOMType GetObjectType() const override
        { return FBOMObjectType(TypeNameWithoutNamespace<HypClassInstanceStub>().Data()); }

    virtual TypeID GetTypeID() const override final
        { return TypeID::ForType<HypClassInstanceStub>(); }

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject &out) const override;
    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out) const override;
};

} // namespace hyperion::fbom

#endif
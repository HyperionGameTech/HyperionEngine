/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <scene/Entity.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Entity> : public FBOMObjectMarshalerBase<Entity>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Entity &in_object, FBOMObject &out) const override
    {
        // @TODO

        return { FBOMResult::FBOM_ERR, "Not implemented" };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        // @TODO

        return { FBOMResult::FBOM_ERR, "Not implemented" };
    }
};

HYP_DEFINE_MARSHAL(Entity, FBOMMarshaler<Entity>);

} // namespace hyperion::fbom
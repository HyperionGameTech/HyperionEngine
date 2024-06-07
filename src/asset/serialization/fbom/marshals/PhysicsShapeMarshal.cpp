/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <physics/RigidBody.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

using namespace hyperion::physics;

template <>
class FBOMMarshaler<PhysicsShape> : public FBOMObjectMarshalerBase<PhysicsShape>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const PhysicsShape &in_object, FBOMObject &out) const override
    {
        /// @TODO: Serialize the actual object -- will be dependent on the physics engine used

        out.SetProperty("type", FBOMData::FromUnsignedInt(uint32(in_object.GetType())));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        uint32 type;

        if (auto err = in.GetProperty("type").ReadUnsignedInt(&type)) {
            return err;
        }

        out_object = RC<PhysicsShape>::Construct(PhysicsShapeType(type));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(PhysicsShape, FBOMMarshaler<PhysicsShape>);

} // namespace hyperion::fbom
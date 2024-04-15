/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_FBOM_MARSHALS_PHYSICS_SHAPE_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_PHYSICS_SHAPE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <physics/RigidBody.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<physics::PhysicsShape> : public FBOMObjectMarshalerBase<physics::PhysicsShape>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const physics::PhysicsShape &in_object, FBOMObject &out) const override
    {
        /// @TODO: Serialize the actual object -- will be dependent on the physics engine used

        out.SetProperty("type", FBOMData::FromUnsignedInt(uint32(in_object.GetType())));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        uint32 type;

        if (auto err = in.GetProperty("type").ReadUnsignedInt(&type)) {
            return err;
        }

        out_object = UniquePtr<physics::PhysicsShape>::Construct(physics::PhysicsShapeType(type));

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif
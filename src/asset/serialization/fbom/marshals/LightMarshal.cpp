/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <rendering/Light.hpp>

#include <core/object/HypData.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Light> : public FBOMObjectMarshalerBase<Light>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Light &in_object, FBOMObject &out) const override
    {
        out.SetProperty(NAME("type"), FBOMData::FromUInt32(uint32(in_object.GetType())));
        out.SetProperty(NAME("position"), FBOMData::FromVec3f(in_object.GetPosition()));
        out.SetProperty(NAME("normal"), FBOMData::FromVec3f(in_object.GetNormal()));
        out.SetProperty(NAME("area_size"), FBOMData::FromVec2f(in_object.GetAreaSize()));
        out.SetProperty(NAME("color"), FBOMData::FromUInt32(uint32(in_object.GetColor())));
        out.SetProperty(NAME("intensity"), FBOMData::FromFloat(in_object.GetIntensity()));
        out.SetProperty(NAME("radius"), FBOMData::FromFloat(in_object.GetRadius()));
        out.SetProperty(NAME("falloff"), FBOMData::FromFloat(in_object.GetFalloff()));
        out.SetProperty(NAME("spot_angles"), FBOMData::FromVec2f(in_object.GetSpotAngles()));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        // @TODO

        float intensity = 0.0f;
        in.GetProperty("intensity").ReadFloat(&intensity);

        float radius;
        in.GetProperty("radius").ReadFloat(&radius);

        out = HypData(CreateObject<Light>(
            LightType::POINT,
            Vec3f { },
            Color { },
            intensity,
            radius
        ));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Light, FBOMMarshaler<Light>);

} // namespace hyperion::fbom
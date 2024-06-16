/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <rendering/Light.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Light> : public FBOMObjectMarshalerBase<Light>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Light &in_object, FBOMObject &out) const override
    {
        out.SetProperty(NAME("type"), FBOMData(uint32(in_object.GetType())));
        out.SetProperty(NAME("position"), FBOMData(in_object.GetPosition()));
        out.SetProperty(NAME("normal"), FBOMData(in_object.GetNormal()));
        out.SetProperty(NAME("area_size"), FBOMData(in_object.GetAreaSize()));
        out.SetProperty(NAME("color"), FBOMData(uint32(in_object.GetColor())));
        out.SetProperty(NAME("intensity"), FBOMData(in_object.GetIntensity()));
        out.SetProperty(NAME("radius"), FBOMData(in_object.GetRadius()));
        out.SetProperty(NAME("falloff"), FBOMData(in_object.GetFalloff()));
        out.SetProperty(NAME("spot_angles"), FBOMData(in_object.GetSpotAngles()));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        // @TODO

        out_object = CreateObject<Light>(
            LightType::SPOT,
            Vec3f { },
            Color { },
            0.0f,
            0.0f
        );

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Light, FBOMMarshaler<Light>);

} // namespace hyperion::fbom
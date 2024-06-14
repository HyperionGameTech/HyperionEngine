/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<TextureDesc> : public FBOMObjectMarshalerBase<TextureDesc>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const TextureDesc &desc, FBOMObject &out) const override
    {
        out.SetProperty(NAME("type"), FBOMData::FromUnsignedInt(uint32(desc.type)));
        out.SetProperty(NAME("format"), FBOMData::FromUnsignedInt(uint32(desc.format)));
        out.SetProperty(NAME("extent"), FBOMStruct::Create<Extent3D>(), &desc.extent);
        out.SetProperty(NAME("filter_mode_min"), FBOMData::FromUnsignedInt(uint32(desc.filter_mode_min)));
        out.SetProperty(NAME("filter_mode_mag"), FBOMData::FromUnsignedInt(uint32(desc.filter_mode_mag)));
        out.SetProperty(NAME("wrap_mode"), FBOMData::FromUnsignedInt(uint32(desc.wrap_mode)));
        out.SetProperty(NAME("num_layers"), FBOMData::FromUnsignedInt(uint32(desc.num_layers)));
        out.SetProperty(NAME("num_faces"), FBOMData::FromUnsignedInt(uint32(desc.num_faces)));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        TextureDesc result;

        in.GetProperty("type").ReadUnsignedInt(&result.type);
        in.GetProperty("format").ReadUnsignedInt(&result.format);
        in.GetProperty("extent").ReadStruct<Extent3D>(&result.extent);
        in.GetProperty("filter_mode_min").ReadUnsignedInt(&result.filter_mode_min);
        in.GetProperty("filter_mode_mag").ReadUnsignedInt(&result.filter_mode_mag);
        in.GetProperty("wrap_mode").ReadUnsignedInt(&result.wrap_mode);
        in.GetProperty("num_layers").ReadUnsignedInt(&result.num_layers);
        in.GetProperty("num_faces").ReadUnsignedInt(&result.num_faces);

        out_object = result;

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(TextureDesc, FBOMMarshaler<TextureDesc>);

} // namespace hyperion::fbom
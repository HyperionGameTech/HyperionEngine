/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <core/object/HypData.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<TextureDesc> : public FBOMObjectMarshalerBase<TextureDesc>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const TextureDesc &desc, FBOMObject &out) const override
    {
        out.SetProperty("type", FBOMData::FromUInt32(uint32(desc.type)));
        out.SetProperty("format", FBOMData::FromUInt32(uint32(desc.format)));
        out.SetProperty("extent", FBOMStruct::Create<Extent3D>(), &desc.extent);
        out.SetProperty("filter_mode_min", FBOMData::FromUInt32(uint32(desc.filter_mode_min)));
        out.SetProperty("filter_mode_mag", FBOMData::FromUInt32(uint32(desc.filter_mode_mag)));
        out.SetProperty("wrap_mode", FBOMData::FromUInt32(uint32(desc.wrap_mode)));
        out.SetProperty("num_layers", FBOMData::FromUInt32(uint32(desc.num_layers)));
        out.SetProperty("num_faces", FBOMData::FromUInt32(uint32(desc.num_faces)));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        TextureDesc result;

        in.GetProperty("type").ReadUInt32(&result.type);
        in.GetProperty("format").ReadUInt32(&result.format);
        in.GetProperty("extent").ReadStruct<Extent3D>(&result.extent);
        in.GetProperty("filter_mode_min").ReadUInt32(&result.filter_mode_min);
        in.GetProperty("filter_mode_mag").ReadUInt32(&result.filter_mode_mag);
        in.GetProperty("wrap_mode").ReadUInt32(&result.wrap_mode);
        in.GetProperty("num_layers").ReadUInt32(&result.num_layers);
        in.GetProperty("num_faces").ReadUInt32(&result.num_faces);

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(TextureDesc, FBOMMarshaler<TextureDesc>);

} // namespace hyperion::fbom
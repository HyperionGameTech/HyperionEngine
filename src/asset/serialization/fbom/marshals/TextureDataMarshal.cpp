/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<TextureData> : public FBOMObjectMarshalerBase<TextureData>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const TextureData &data, FBOMObject &out) const override
    {
        out.SetProperty(NAME("desc"), FBOMData::FromObject(
            FBOMObject()
                .SetProperty(NAME("type"), FBOMUnsignedInt(), uint32(data.desc.type))
                .SetProperty(NAME("format"), FBOMUnsignedInt(), uint32(data.desc.format))
                .SetProperty(NAME("extent"), FBOMStruct::Create<Extent3D>(), &data.desc.extent)
                .SetProperty(NAME("filter_mode_min"), FBOMUnsignedInt(), uint32(data.desc.filter_mode_min))
                .SetProperty(NAME("filter_mode_mag"), FBOMUnsignedInt(), uint32(data.desc.filter_mode_mag))
                .SetProperty(NAME("wrap_mode"), FBOMUnsignedInt(), uint32(data.desc.wrap_mode))
                .SetProperty(NAME("num_layers"), FBOMUnsignedInt(), uint32(data.desc.num_layers))
                .SetProperty(NAME("num_faces"), FBOMUnsignedInt(), uint32(data.desc.num_faces))
        ));

        out.SetProperty(NAME("buffer"), FBOMByteBuffer(), data.buffer);

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        TextureData result;

        FBOMObject desc_object;
        
        if (FBOMResult err = in.GetProperty("desc").ReadObject(desc_object)) {
            return err;
        }

        desc_object.GetProperty("type").ReadUnsignedInt(&result.desc.type);
        desc_object.GetProperty("format").ReadUnsignedInt(&result.desc.format);
        desc_object.GetProperty("extent").ReadStruct<Extent3D>(&result.desc.extent);
        desc_object.GetProperty("filter_mode_min").ReadUnsignedInt(&result.desc.filter_mode_min);
        desc_object.GetProperty("filter_mode_mag").ReadUnsignedInt(&result.desc.filter_mode_mag);
        desc_object.GetProperty("wrap_mode").ReadUnsignedInt(&result.desc.wrap_mode);
        desc_object.GetProperty("num_layers").ReadUnsignedInt(&result.desc.num_layers);
        desc_object.GetProperty("num_faces").ReadUnsignedInt(&result.desc.num_faces);

        if (FBOMResult err = in.GetProperty("buffer").ReadByteBuffer(result.buffer)) {
            return err;
        }

        out_object = std::move(result);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(TextureData, FBOMMarshaler<TextureData>);

} // namespace hyperion::fbom
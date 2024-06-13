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
        out.AddChild(data.desc);

        out.SetProperty(NAME("buffer"), FBOMByteBuffer(), data.buffer);

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        TextureData result;

        const auto desc_it = in.nodes->FindIf([](const FBOMObject &item)
        {
            return item.GetType().IsOrExtends("TextureDesc");
        });

        if (desc_it == in.nodes->End()) {
            return { FBOMResult::FBOM_ERR, "No TextureDesc child object on TextureData" };
        }

        if (!desc_it->deserialized.Get<TextureDesc>()) {
            return { FBOMResult::FBOM_ERR, "Invalid TextureDesc child object on TextureData" };
        }

        result.desc = *desc_it->deserialized.Get<TextureDesc>();

        if (FBOMResult err = in.GetProperty("buffer").ReadByteBuffer(result.buffer)) {
            return err;
        }

        out_object = std::move(result);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(TextureData, FBOMMarshaler<TextureData>);

} // namespace hyperion::fbom
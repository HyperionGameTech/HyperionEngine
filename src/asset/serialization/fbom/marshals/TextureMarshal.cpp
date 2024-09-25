/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <rendering/Texture.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <core/object/HypData.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Texture> : public FBOMObjectMarshalerBase<Texture>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const Texture &in_object, FBOMObject &out) const override
    {
        if (in_object.GetImage()->HasAssignedImageData()) {
            auto ref = in_object.GetImage()->GetStreamedData()->AcquireRef();
            const TextureData &texture_data = ref->GetTextureData();

            out.AddChild(texture_data);
        } else {
            const TextureDesc &desc = in_object.GetTextureDesc();

            out.AddChild(desc);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        const auto texture_data_it = in.nodes->FindIf([](const FBOMObject &item)
        {
            return item.GetType().IsOrExtends("TextureData");
        });

        if (texture_data_it != in.nodes->End()) {
            out = HypData(CreateObject<Texture>(RC<StreamedTextureData>(new StreamedTextureData(
                texture_data_it->m_deserialized_object->Get<TextureData>()
            ))));

            return { FBOMResult::FBOM_OK };
        }

        const auto texture_desc_it = in.nodes->FindIf([](const FBOMObject &item)
        {
            return item.GetType().IsOrExtends("TextureDesc");
        });

        if (texture_desc_it != in.nodes->End()) {
            out = HypData(CreateObject<Texture>(texture_desc_it->m_deserialized_object->Get<TextureDesc>()));

            return { FBOMResult::FBOM_OK };
        }

        return { FBOMResult::FBOM_ERR, "No TextureData or TextureDesc on Texture object" };
    }
};

HYP_DEFINE_MARSHAL(Texture, FBOMMarshaler<Texture>);

} // namespace hyperion::fbom
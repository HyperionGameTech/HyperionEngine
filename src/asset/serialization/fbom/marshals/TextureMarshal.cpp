/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <rendering/Texture.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <core/object/HypData.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<Texture> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject &out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out)) {
            return err;
        }

        const Texture &in_object = in.Get<Texture>();

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
        Handle<Texture> texture_handle;

        const auto texture_data_it = in.nodes->FindIf([](const FBOMObject &item)
        {
            return item.GetType().IsOrExtends("TextureData");
        });

        if (texture_data_it != in.nodes->End()) {
            texture_handle = CreateObject<Texture>(MakeRefCountedPtr<StreamedTextureData>(
                texture_data_it->m_deserialized_object->Get<TextureData>()
            ));
        }

        const auto texture_desc_it = in.nodes->FindIf([](const FBOMObject &item)
        {
            return item.GetType().IsOrExtends("TextureDesc");
        });

        if (texture_desc_it != in.nodes->End()) {
            texture_handle = CreateObject<Texture>(texture_desc_it->m_deserialized_object->Get<TextureDesc>());
        }
    
        if (!texture_handle.IsValid()) {
            return { FBOMResult::FBOM_ERR, "No TextureData or TextureDesc on Texture object" };
        }

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(in, Texture::Class(), AnyRef(*texture_handle))) {
            return err;
        }
        
        out = HypData(std::move(texture_handle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Texture, FBOMMarshaler<Texture>);

} // namespace hyperion::fbom
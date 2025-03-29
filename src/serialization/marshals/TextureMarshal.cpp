/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/RenderTexture.hpp>
#include <rendering/backend/RendererImage.hpp>

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

        if (const RC<StreamedTextureData> &streamed_texture_data = in_object.GetStreamedTextureData()) {
            TResourceHandle<StreamedTextureData> resource_handle(*streamed_texture_data);
            
            const TextureData &texture_data = resource_handle->GetTextureData();

            out.AddChild(texture_data);
        } else {
            const TextureDesc &desc = in_object.GetTextureDesc();

            out.AddChild(desc);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(fbom::FBOMLoadContext &context, const FBOMObject &in, HypData &out) const override
    {
        Handle<Texture> texture_handle;

        RC<StreamedTextureData> streamed_texture_data;
        TextureDesc texture_desc;

        const auto texture_data_it = in.GetChildren().FindIf([](const FBOMObject &item)
        {
            return item.GetType().IsOrExtends("TextureData");
        });

        if (texture_data_it != in.GetChildren().End()) {
            streamed_texture_data = MakeRefCountedPtr<StreamedTextureData>(
                texture_data_it->m_deserialized_object->Get<TextureData>()
            );
        }

        const auto texture_desc_it = in.GetChildren().FindIf([](const FBOMObject &item)
        {
            return item.GetType().IsOrExtends("TextureDesc");
        });

        if (texture_desc_it != in.GetChildren().End()) {
            texture_desc = texture_desc_it->m_deserialized_object->Get<TextureDesc>();
        }

        if (streamed_texture_data != nullptr) {
            texture_handle = CreateObject<Texture>(streamed_texture_data);
        } else {
            texture_handle = CreateObject<Texture>(texture_desc);
        }
    
        if (!texture_handle.IsValid()) {
            return { FBOMResult::FBOM_ERR, "No TextureData or TextureDesc on Texture object" };
        }

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, Texture::Class(), AnyRef(*texture_handle))) {
            return err;
        }
        
        out = HypData(std::move(texture_handle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Texture, FBOMMarshaler<Texture>);

} // namespace hyperion::fbom
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <core/object/HypData.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<TextureData> : public FBOMObjectMarshalerBase<TextureData>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const TextureData& data, FBOMObject& out) const override
    {
        out.AddChild(data.desc);

        out.SetProperty("Buffer", FBOMData::FromByteBuffer(data.buffer, FBOMDataFlags::COMPRESSED));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(fbom::FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        TextureData result;

        const auto desc_it = in.GetChildren().FindIf([](const FBOMObject& item)
            {
                return item.GetType().IsOrExtends("TextureDesc");
            });

        if (desc_it == in.GetChildren().End())
        {
            return { FBOMResult::FBOM_ERR, "No TextureDesc child object on TextureData" };
        }

        result.desc = desc_it->m_deserialized_object->Get<TextureDesc>();

        if (FBOMResult err = in.GetProperty("Buffer").ReadByteBuffer(result.buffer))
        {
            return err;
        }

        out = HypData(std::move(result));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(TextureData, FBOMMarshaler<TextureData>);

} // namespace hyperion::fbom
/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>

#include <rendering/Shared.hpp>

namespace hyperion {

HYP_CLASS()
class TextureAsset : public AssetObject
{
    HYP_OBJECT_BODY(TextureAsset);

public:
    TextureAsset()
        : AssetObject(),
          m_textureDesc()
    {
    }

    TextureAsset(Name name, const TextureDesc& desc)
        : AssetObject(name),
          m_textureDesc(desc)
    {
    }

    TextureAsset(Name name, const TextureDesc& desc, const TextureData& textureData)
        : AssetObject(name, textureData),
          m_textureDesc(desc)
    {
        AssertDebug(textureData.imageData.Size() != 0);
    }

    TextureAsset(Name name, const TextureDesc& desc, TextureData&& textureData)
        : AssetObject(name, std::move(textureData)),
          m_textureDesc(desc)
    {
        AssertDebug(GetTextureData()->imageData.Size() != 0);
    }

    TextureAsset(const TextureAsset& other) = delete;
    TextureAsset& operator=(const TextureAsset& other) = delete;

    TextureAsset(TextureAsset&& other) noexcept = delete;
    TextureAsset& operator=(TextureAsset&& other) noexcept = delete;

    ~TextureAsset() = default;

    HYP_FORCE_INLINE const TextureDesc& GetTextureDesc() const
    {
        return m_textureDesc;
    }

    HYP_FORCE_INLINE const TextureData* GetTextureData() const
    {
        return GetResourceData<TextureData>();
    }

private:
    HYP_FIELD(Serialize)
    TextureDesc m_textureDesc;
};

} // namespace hyperion

/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetRegistry.hpp>

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
        AssetObject::SetData(TextureData());
    }

    TextureAsset(Name name, const TextureData& textureData)
        : AssetObject(name, textureData),
          m_textureDesc(textureData.desc)
    {
    }

    TextureAsset(Name name, TextureData&& textureData)
        : AssetObject(name, std::move(textureData)),
          m_textureDesc(textureData.desc)
    {
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

    HYP_FORCE_INLINE TextureData* GetTextureData() const
    {
        return GetResourceData<TextureData>();
    }

private:
    TextureDesc m_textureDesc;
};

} // namespace hyperion

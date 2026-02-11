/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>

#include <rendering/Shared.hpp>

namespace Hyperion {

struct TextureData2
{
    static constexpr uint8 Version = 1;
    static constexpr const char Header[] = "TEX";

    uint64 imageDataSize;

    BlobPointer<ubyte> imageData;

    TextureData2() = default;

    static HYP_NODISCARD TextureData2* Allocate(const TextureData2& other, BlobHeader& outHeader)
    {
        return Allocate(
            ConstByteView(&other.imageData[0], other.imageDataSize),
            outHeader);
    }

    static HYP_NODISCARD TextureData2* Allocate(ConstByteView imageData, BlobHeader& outHeader)
    {
        TextureData2 data {};
        data.imageDataSize = imageData.Size();

        TInlineBlobBuilder<TextureData2, 16> builder(&data);

        return builder
            .Append(offsetof(TextureData2, imageData), imageData.ToSpan())
            .Build(outHeader);
    }
};

HYP_CLASS()
class TextureAsset : public AssetObject
{
    HYP_OBJECT_BODY(TextureAsset);

public:
    TextureAsset()
        : AssetObject(),
          m_textureDesc()
    {
        ConstructBlobData<TextureData2>(TextureData2 {});
    }

    TextureAsset(Name name, const TextureDesc& desc)
        : AssetObject(name),
          m_textureDesc(desc)
    {
        ConstructBlobData<TextureData2>(TextureData2 {});
    }

    TextureAsset(Name name, const TextureDesc& desc, ConstByteView imageData)
        : AssetObject(name),
          m_textureDesc(desc)
    {
        ConstructBlobData<TextureData2>(imageData);
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

    HYP_FORCE_INLINE const TextureData2* GetTextureData() const
    {
        return GetResourceData<TextureData2>();
    }

private:
    HYP_FIELD(Serialize)
    TextureDesc m_textureDesc;
};

} // namespace Hyperion

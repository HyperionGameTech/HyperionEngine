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
    }

    TextureAsset(Name name, const TextureDesc& desc)
        : AssetObject(name),
          m_textureDesc(desc)
    {
    }

    TextureAsset(Name name, const TextureDesc& desc, ConstByteView imageData)
        : AssetObject(name),
          m_textureDesc(desc)
    {
        AllocateBlobData(m_imageData, imageData);
    }

    TextureAsset(const TextureAsset& other) = delete;
    TextureAsset& operator=(const TextureAsset& other) = delete;

    TextureAsset(TextureAsset&& other) noexcept = delete;
    TextureAsset& operator=(TextureAsset&& other) noexcept = delete;

    ~TextureAsset()
    {
        FreeBlobData(m_imageData);
    }

    HYP_FORCE_INLINE const TextureDesc& GetTextureDesc() const
    {
        return m_textureDesc;
    }

    HYP_FORCE_INLINE ConstByteView GetImageData() const
    {
        return m_imageData.raw != nullptr
            ? ConstByteView(reinterpret_cast<const ubyte*>(m_imageData.raw), m_imageData.size)
            : ConstByteView();
    }

private:
    HYP_FIELD(Serialize)
    TextureDesc m_textureDesc;

    HYP_FIELD(Serialize)
    BlobDataReference m_imageData;
};

} // namespace Hyperion

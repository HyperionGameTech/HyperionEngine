/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>

#include <rendering/Shared.hpp>

namespace Hyperion {

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

protected:
    void WriteBlobData(BlobStorage& blobStorage) override
    {
        Assert(m_imageData.raw != nullptr);

        if (!m_imageData.readOnly)
        {
            BlobHeader vertexDataHeader {};
            Memory::Copy(vertexDataHeader.magic, "TEX", 4);
            vertexDataHeader.version = 1;
            vertexDataHeader.payloadOffset = 0;
            vertexDataHeader.payloadSize = m_imageData.size;

            BlobResourceKey key {};

            if (blobStorage.AllocateBlob(vertexDataHeader, key))
            {
                m_imageData.bufferOffset = key.offset;
            }
            else
            {
                return;
            }
        }
        
        ByteWriter* writeStream = blobStorage.GetWriteStream();

        writeStream->Seek(m_imageData.bufferOffset);
        writeStream->Write(m_imageData.raw, m_imageData.size);
    }

    void ReadBlobData(BlobStorage& blobStorage) override
    {
        HYP_NOT_IMPLEMENTED();
    }

private:
    HYP_FIELD(Serialize)
    TextureDesc m_textureDesc;

    HYP_FIELD(Serialize)
    BlobDataReference m_imageData;
};

} // namespace Hyperion

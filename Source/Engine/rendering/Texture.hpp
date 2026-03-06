/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>

#include <asset/AssetReference.hpp>
#include <asset/AssetObject.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <Core/containers/FixedArray.hpp>

#include <Core/utilities/Pair.hpp>

#include <Core/io/ByteReader.hpp>
#include <Core/io/ByteWriter.hpp>

#include <Core/math/Vector3.hpp>

namespace Hyperion {

HYP_CLASS()
class HYP_API Texture : public AssetObject
{
    HYP_OBJECT_BODY(Texture);

public:
    static const FixedArray<Pair<Vec3f, Vec3f>, 6> s_cubemapDirections;

    Texture();

    explicit Texture(const TextureDesc& textureDesc);
    Texture(const TextureDesc& textureDesc, ConstByteView imageData);

    Texture(const Texture& other) = delete;
    Texture& operator=(const Texture& other) = delete;

    Texture(Texture&& other) noexcept = delete;
    Texture& operator=(Texture&& other) noexcept = delete;

    ~Texture() override;

    HYP_METHOD()
    virtual Result Rename(Name name) override;

    HYP_FORCE_INLINE const TextureDesc& GetTextureDesc() const
    {
        return m_textureDesc;
    }

    HYP_FORCE_INLINE void SetTextureDesc(const TextureDesc& textureDesc)
    {
        m_textureDesc = textureDesc;
    }

    HYP_FORCE_INLINE TextureType GetType() const
    {
        return GetTextureDesc().type;
    }

    HYP_FORCE_INLINE uint32 NumArrayLayers() const
    {
        return GetTextureDesc().NumArrayLayers();
    }

    HYP_FORCE_INLINE bool IsTextureCube() const
    {
        return GetTextureDesc().IsTextureCube();
    }

    HYP_FORCE_INLINE bool IsPanorama() const
    {
        return GetTextureDesc().IsPanorama();
    }

    HYP_FORCE_INLINE const Vec3u& GetExtent() const
    {
        return GetTextureDesc().extent;
    }

    HYP_FORCE_INLINE TextureFormat GetFormat() const
    {
        return GetTextureDesc().format;
    }

    HYP_FORCE_INLINE HYP_DEPRECATED TextureFilterMode GetFilterMode() const
    {
        return GetTextureDesc().filterModeMin;
    }

    HYP_FORCE_INLINE TextureFilterMode GetMinFilterMode() const
    {
        return GetTextureDesc().filterModeMin;
    }

    HYP_FORCE_INLINE TextureFilterMode GetMagFilterMode() const
    {
        return GetTextureDesc().filterModeMag;
    }

    HYP_FORCE_INLINE bool HasMipMaps() const
    {
        return GetTextureDesc().HasMipMaps();
    }

    HYP_FORCE_INLINE TextureWrapMode GetWrapMode() const
    {
        return GetTextureDesc().wrapMode;
    }

    HYP_FORCE_INLINE const GpuImageRef& GetGpuImage() const
    {
        return m_gpuImage;
    }

    HYP_FORCE_INLINE ConstByteView GetImageData() const
    {
        return ConstByteView(reinterpret_cast<const ubyte*>(m_imageData.raw), m_imageData.size);
    }

    void SetImageData(ConstByteView imageData);

    static void GenerateMipmaps(TextureDesc& desc, ByteBuffer& imageData);

    /*! \brief Blocking call to readback GPU image data into a CPU-side buffer. Must be called on the render thread.
     *  Do not use frequently as this will stall the gpu */
    void Readback(ByteBuffer& outByteBuffer);

    /*! \brief Enqueues commands to read GPU image data into a CPU-side buffer. Must be called on the render thread.
     *  The callback will be called when the current frame is no longer being used by the GPU. If no current frame exists,
     *  Readback() will be called instead. */
    void EnqueueReadback(Proc<void(ByteBuffer&& byteBuffer)>&& callback);

    Vec4f Sample(Vec3f uvw, uint32 faceIndex);
    Vec4f Sample2D(Vec2f uv);
    Vec4f SampleCube(Vec3f direction);

protected:
    void Init() override;

    void PageBlobData() override;
    void UnpageBlobData() override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        outReferences.EmplaceBack("TEX", 1, &m_imageData);
    }

    HYP_FIELD(Serialize)
    TextureDesc m_textureDesc;

    HYP_FIELD(Serialize)
    BlobDataReference m_imageData;

    HYP_FIELD(Transient)
    GpuImageRef m_gpuImage;
};

} // namespace Hyperion

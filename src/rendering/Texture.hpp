/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetReference.hpp>
#include <asset/AssetObject.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/utilities/Pair.hpp>

#include <core/reflection/HypObjectBase.hpp>

#include <core/math/Vector3.hpp>

#include <core/Types.hpp>

namespace hyperion {

class TextureAsset;

HYP_CLASS()
class HYP_API Texture final : public AssetObject
{
    HYP_OBJECT_BODY(Texture);

public:
    static const FixedArray<Pair<Vec3f, Vec3f>, 6> s_cubemapDirections;

    Texture();

    explicit Texture(const TextureDesc& textureDesc);
    Texture(const TextureDesc& textureDesc, const TextureData& textureData);

    explicit Texture(const Handle<TextureAsset>& asset);

    Texture(const Texture& other) = delete;
    Texture& operator=(const Texture& other) = delete;

    Texture(Texture&& other) noexcept = delete;
    Texture& operator=(Texture&& other) noexcept = delete;

    ~Texture() override;

    HYP_METHOD()
    virtual Result Rename(Name name) override;

    const Handle<TextureAsset>& GetAsset() const;

    HYP_METHOD(Property = "AssetReference")
    const AssetReference& GetAssetReference() const
    {
        return m_assetReference;
    }

    const TextureDesc& GetTextureDesc() const;
    void SetTextureDesc(const TextureDesc& textureDesc);

    HYP_FORCE_INLINE TextureType GetType() const
    {
        return GetTextureDesc().type;
    }

    HYP_FORCE_INLINE uint32 NumFaces() const
    {
        return GetTextureDesc().NumFaces();
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

    HYP_FORCE_INLINE bool HasMipmaps() const
    {
        return GetTextureDesc().HasMipmaps();
    }

    HYP_FORCE_INLINE TextureWrapMode GetWrapMode() const
    {
        return GetTextureDesc().wrapMode;
    }

    HYP_FORCE_INLINE const GpuImageRef& GetGpuImage() const
    {
        return m_gpuImage;
    }

    void GenerateMipmaps();

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

    /*! \internal Serialization only */
    HYP_METHOD(Property = "AssetReference")
    void SetAssetReference(const AssetReference& assetReference)
    {
        m_assetReference = TAssetReference<TextureAsset>(assetReference);
    }

    TAssetReference<TextureAsset> m_assetReference;

    HYP_FIELD(Transient)
    GpuImageRef m_gpuImage;
};

} // namespace hyperion

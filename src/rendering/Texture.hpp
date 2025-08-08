/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/UniquePtr.hpp>
#include <core/memory/Pimpl.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/threading/Mutex.hpp>

#include <core/utilities/Pair.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuImage.hpp>
#include <rendering/RenderGpuImageView.hpp>

#include <scene/VisibilityState.hpp>

#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <core/Types.hpp>

namespace hyperion {

class TextureAsset;

HYP_CLASS()
class HYP_API Texture final : public HypObjectBase
{
    HYP_OBJECT_BODY(Texture);

public:
    static const FixedArray<Pair<Vec3f, Vec3f>, 6> cubemapDirections;

    Texture();

    Texture(const TextureDesc& textureDesc);
    Texture(const TextureData& textureData);

    Texture(const Handle<TextureAsset>& asset);

    Texture(const Texture& other) = delete;
    Texture& operator=(const Texture& other) = delete;

    Texture(Texture&& other) noexcept = delete;
    Texture& operator=(Texture&& other) noexcept = delete;

    ~Texture();

    HYP_METHOD(Property = "Name", Serialize = true, Editor = true)
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name", Serialize = true, Editor = true)
    void SetName(Name name);

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

    HYP_FORCE_INLINE const Handle<TextureAsset>& GetAsset() const
    {
        return m_asset;
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

    void UploadGpuData();

    /*! \brief Readback() implementation, without locking mutex. */
    void Readback_Internal();

    Name m_name;

    Handle<TextureAsset> m_asset;

    GpuImageRef m_gpuImage;
};

} // namespace hyperion


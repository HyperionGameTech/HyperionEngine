/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/Array.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderGpuImage.hpp>

#include <core/Types.hpp>

namespace hyperion {
enum class RenderPassStage : uint8
{
    NONE,
    PRESENT, /* for presentation on screen */
    SHADER   /* for use as a sampled texture in a shader */
};

enum class LoadOperation : uint8
{
    UNDEFINED,
    NONE,
    CLEAR,
    LOAD
};

enum class StoreOperation : uint8
{
    UNDEFINED,
    NONE,
    STORE
};

class AttachmentBase : public RenderObject<AttachmentBase>
{
public:
    virtual ~AttachmentBase() override = default;

    HYP_FORCE_INLINE const GpuImageRef& GetImage() const
    {
        return m_image;
    }

    HYP_FORCE_INLINE const GpuImageViewRef& GetImageView() const
    {
        return m_imageView;
    }

    HYP_FORCE_INLINE TextureFormat GetFormat() const
    {
        return m_image ? m_image->GetTextureFormat() : TF_NONE;
    }

    HYP_FORCE_INLINE bool IsDepthAttachment() const
    {
        return m_image && m_image->GetTextureDesc().IsDepthStencil();
    }

    HYP_FORCE_INLINE LoadOperation GetLoadOperation() const
    {
        return m_loadOperation;
    }

    HYP_FORCE_INLINE StoreOperation GetStoreOperation() const
    {
        return m_storeOperation;
    }

    HYP_FORCE_INLINE const BlendFunction& GetBlendFunction() const
    {
        return m_blendFunction;
    }

    HYP_FORCE_INLINE void SetBlendFunction(const BlendFunction& blendFunction)
    {
        m_blendFunction = blendFunction;
    }

    HYP_FORCE_INLINE Vec4f GetClearColor() const
    {
        return m_clearColor;
    }

    HYP_FORCE_INLINE void SetClearColor(const Vec4f& clearColor)
    {
        m_clearColor = clearColor;
    }

    HYP_FORCE_INLINE uint32 GetBinding() const
    {
        return m_binding;
    }

    HYP_FORCE_INLINE void SetBinding(uint32 binding)
    {
        m_binding = binding;
    }

    HYP_FORCE_INLINE bool HasBinding() const
    {
        return m_binding != MathUtil::MaxSafeValue<uint32>();
    }

    HYP_FORCE_INLINE const FramebufferWeakRef& GetFramebuffer() const
    {
        return m_framebuffer;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;
    virtual RendererResult Destroy() = 0;

protected:
    AttachmentBase(
        const GpuImageRef& image,
        const FramebufferWeakRef& framebuffer,
        LoadOperation loadOperation,
        StoreOperation storeOperation,
        BlendFunction blendFunction)
        : m_image(image),
          m_framebuffer(framebuffer),
          m_loadOperation(loadOperation),
          m_storeOperation(storeOperation),
          m_blendFunction(blendFunction),
          m_clearColor(Vec4f::Zero())
    {
    }

    GpuImageRef m_image;
    GpuImageViewRef m_imageView;

    FramebufferWeakRef m_framebuffer;

    LoadOperation m_loadOperation;
    StoreOperation m_storeOperation;

    BlendFunction m_blendFunction;

    Vec4f m_clearColor;

    uint32 m_binding = MathUtil::MaxSafeValue<uint32>();
};

} // namespace hyperion

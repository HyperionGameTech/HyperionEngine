/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_ATTACHMENT_HPP
#define HYPERION_BACKEND_RENDERER_ATTACHMENT_HPP

#include <core/Defines.hpp>

#include <core/containers/Array.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/Platform.hpp>

#include <Types.hpp>

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

    HYP_FORCE_INLINE const ImageRef& GetImage() const
    {
        return m_image;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetImageView() const
    {
        return m_image_view;
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
        return m_load_operation;
    }

    HYP_FORCE_INLINE StoreOperation GetStoreOperation() const
    {
        return m_store_operation;
    }

    HYP_FORCE_INLINE const BlendFunction& GetBlendFunction() const
    {
        return m_blend_function;
    }

    HYP_FORCE_INLINE void SetBlendFunction(const BlendFunction& blend_function)
    {
        m_blend_function = blend_function;
    }

    HYP_FORCE_INLINE Vec4f GetClearColor() const
    {
        return m_clear_color;
    }

    HYP_FORCE_INLINE void SetClearColor(const Vec4f& clear_color)
    {
        m_clear_color = clear_color;
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

    HYP_FORCE_INLINE bool AllowBlending() const
    {
        return m_allow_blending;
    }

    HYP_FORCE_INLINE void SetAllowBlending(bool allow_blending)
    {
        m_allow_blending = allow_blending;
    }

    HYP_FORCE_INLINE const FramebufferWeakRef& GetFramebuffer() const
    {
        return m_framebuffer;
    }

    virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

protected:
    AttachmentBase(
        const ImageRef& image,
        const FramebufferWeakRef& framebuffer,
        LoadOperation load_operation,
        StoreOperation store_operation,
        BlendFunction blend_function)
        : m_image(image),
          m_framebuffer(framebuffer),
          m_load_operation(load_operation),
          m_store_operation(store_operation),
          m_blend_function(blend_function),
          m_clear_color(Vec4f::Zero())
    {
    }

    ImageRef m_image;
    ImageViewRef m_image_view;

    FramebufferWeakRef m_framebuffer;

    LoadOperation m_load_operation;
    StoreOperation m_store_operation;

    BlendFunction m_blend_function;

    Vec4f m_clear_color;

    uint32 m_binding = MathUtil::MaxSafeValue<uint32>();

    bool m_allow_blending = true;
};

} // namespace hyperion

#endif
/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/containers/Array.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Shared.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class AttachmentBase : public Texture
{
    HYP_OBJECT_BODY(AttachmentBase);

public:
    virtual ~AttachmentBase() override = default;
    
    static Pool* GetAllocator() { return g_rhiPool; }

    HYP_FORCE_INLINE const GpuImageViewRef& GetImageView() const
    {
        return m_imageView;
    }

    HYP_FORCE_INLINE bool IsDepthAttachment() const
    {
        return GetTextureDesc().IsDepthStencil();
    }

    HYP_FORCE_INLINE const AttachmentDesc& GetAttachmentDesc() const
    {
        return m_attachmentDesc;
    }

    HYP_FORCE_INLINE LoadOperation GetLoadOperation() const
    {
        return m_attachmentDesc.loadOp;
    }

    HYP_FORCE_INLINE StoreOperation GetStoreOperation() const
    {
        return m_attachmentDesc.storeOp;
    }

    HYP_FORCE_INLINE const BlendFunction& GetBlendFunction() const
    {
        return m_attachmentDesc.blendFunction;
    }

    HYP_FORCE_INLINE void SetBlendFunction(const BlendFunction& blendFunction)
    {
        m_attachmentDesc.blendFunction = blendFunction;
    }

    HYP_FORCE_INLINE Vec4f GetClearColor() const
    {
        return Vec4f(
            m_attachmentDesc.clearColor[0],
            m_attachmentDesc.clearColor[1],
            m_attachmentDesc.clearColor[2],
            m_attachmentDesc.clearColor[3]);
    }

    HYP_FORCE_INLINE void SetClearColor(const Vec4f& clearColor)
    {
        m_attachmentDesc.clearColor[0] = clearColor[0];
        m_attachmentDesc.clearColor[1] = clearColor[1];
        m_attachmentDesc.clearColor[2] = clearColor[2];
        m_attachmentDesc.clearColor[3] = clearColor[3];
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

protected:
    AttachmentBase(
        const GpuImageRef& image,
        const GpuImageViewRef& imageView, // May be null
        const FramebufferWeakRef& framebuffer,
        const AttachmentDesc& attachmentDesc)
        : Texture(),
          m_imageView(imageView),
          m_framebuffer(framebuffer),
          m_attachmentDesc(attachmentDesc)
    {
        m_gpuImage = image;
    }

    GpuImageViewRef m_imageView;

    FramebufferWeakRef m_framebuffer;

    AttachmentDesc m_attachmentDesc;

    uint32 m_binding = MathUtil::MaxSafeValue<uint32>();
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanAttachment.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12Attachment.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
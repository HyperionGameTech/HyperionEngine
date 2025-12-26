/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Constants.hpp>
#include <core/Types.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

namespace Hyperion {

class Mesh;
class Texture;
class RenderGroup;
class TemporalBlending;
class GBuffer;
struct RenderSetup;
enum RenderTargetType : uint8;

enum FullScreenPassFlags : uint32
{
    FSP_NONE = 0x0,
    FSP_EXTERNAL_RENDERTARGET = 0x1, //!< Use external target, don't create our own.
    FSP_RENDERTARGET_LOAD = 0x2      //!< Target should use LOAD op rather than CLEAR to preserve framebuffer contents.
};

HYP_MAKE_ENUM_FLAGS(FullScreenPassFlags);

HYP_CLASS(NoScriptBindings)
class HYP_API FullScreenPass : public ObjectBase
{
    HYP_OBJECT_BODY(FullScreenPass);

public:
    friend struct RecreateFullScreenPassFramebuffer;

    FullScreenPass(EnumFlags<FullScreenPassFlags> flags = FSP_NONE);

    FullScreenPass(
        TextureFormat imageFormat,
        GBuffer* gbuffer,
        EnumFlags<FullScreenPassFlags> flags = FSP_NONE);

    FullScreenPass(
        TextureFormat imageFormat,
        Vec2u extent,
        GBuffer* gbuffer,
        EnumFlags<FullScreenPassFlags> flags = FSP_NONE);

    FullScreenPass(
        const ShaderRef& shader,
        TextureFormat imageFormat,
        Vec2u extent,
        GBuffer* gbuffer,
        EnumFlags<FullScreenPassFlags> flags = FSP_NONE);

    FullScreenPass(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable,
        TextureFormat imageFormat,
        Vec2u extent,
        GBuffer* gbuffer,
        EnumFlags<FullScreenPassFlags> flags = FSP_NONE);

    FullScreenPass(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptorTable,
        const FramebufferRef& framebuffer,
        TextureFormat imageFormat,
        Vec2u extent,
        GBuffer* gbuffer,
        EnumFlags<FullScreenPassFlags> flags = FSP_NONE);

    FullScreenPass(const FullScreenPass&) = delete;
    FullScreenPass& operator=(const FullScreenPass&) = delete;
    virtual ~FullScreenPass();

    HYP_FORCE_INLINE const Vec2u& GetExtent() const
    {
        return m_extent;
    }

    HYP_FORCE_INLINE TextureFormat GetFormat() const
    {
        return m_imageFormat;
    }

    AttachmentBase* GetAttachment(uint32 attachmentIndex) const;

    HYP_FORCE_INLINE const FramebufferRef& GetFramebuffer() const
    {
        return m_framebuffer;
    }

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    void SetShader(const ShaderRef& shader);

    HYP_FORCE_INLINE const Handle<Mesh>& GetQuadMesh() const
    {
        return m_fullScreenQuad;
    }

    HYP_FORCE_INLINE void SetPushConstants(const PushConstantData& pc)
    {
        m_pushConstantData = pc;
    }

    HYP_FORCE_INLINE void SetPushConstants(const void* ptr, SizeType size)
    {
        SetPushConstants(PushConstantData(ptr, size));
    }

    HYP_FORCE_INLINE const BlendFunction& GetBlendFunction() const
    {
        return m_blendFunction;
    }

    /*! \brief Sets the blend function of the render pass.
        Must be set before Create() is called. */
    void SetBlendFunction(const BlendFunction& blendFunction);

    HYP_FORCE_INLINE RenderTargetType GetRenderTargetType() const
    {
        return m_renderTargetType;
    }

    void SetRenderTargetType(RenderTargetType renderTargetType);

    HYP_FORCE_INLINE const Optional<DescriptorTableRef>& GetDescriptorTable() const
    {
        return m_descriptorTable;
    }

    const GraphicsPipelineRef& GetGraphicsPipeline();

    virtual GpuImageViewRef GetFinalImageView() const;
    virtual GpuImageViewRef GetPreviousFrameColorImageView() const;

    /*! \brief Resizes the full screen pass to the new size.
     *  Callable on any thread, as it enqueues a render command. */
    void Resize(Vec2u newSize);

    virtual void CreateFramebuffer();
    virtual void CreatePipeline(const RenderableAttributeSet& renderableAttributes);
    virtual void CreatePipeline();
    virtual void CreateDescriptors();

    /*! \brief Create the full screen pass */
    virtual void Create();

    virtual void Render(Frame* frame, const RenderSetup& renderSetup);
    void RenderToFramebuffer(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer);

    void Begin(Frame* frame, const RenderSetup& renderSetup);
    void End(Frame* frame, const RenderSetup& renderSetup);

protected:
    virtual bool UsesTemporalBlending() const
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u newSize);

    virtual void Render_Internal(Frame* frame, const RenderSetup& renderSetup, GraphicsPipeline* graphicsPipeline)
    {
    }

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer);

    void CreateQuad();

    void RenderPreviousTextureToScreen(Frame* frame, const RenderSetup& renderSetup);
    void CopyResultToPreviousTexture(Frame* frame, const RenderSetup& renderSetup);
    void MergeHalfResTextures(Frame* frame, const RenderSetup& renderSetup);

    FramebufferRef m_framebuffer;
    ShaderRef m_shader;
    GraphicsPipelineCacheHandle m_graphicsPipelineCacheHandle;
    Handle<Mesh> m_fullScreenQuad;
    Vec2u m_extent;
    GBuffer* m_gbuffer;

    EnumFlags<FullScreenPassFlags> m_flags;

    PushConstantData m_pushConstantData;

    TextureFormat m_imageFormat;

    BlendFunction m_blendFunction;

    RenderTargetType m_renderTargetType;

    Optional<DescriptorTableRef> m_descriptorTable;

    UniquePtr<TemporalBlending> m_temporalBlending;
    Handle<Texture> m_previousTexture;

    Handle<FullScreenPass> m_renderTextureToScreenPass;

    bool m_isFirstFrame;

private:
    void CreateTemporalBlending();
    void CreateRenderTextureToScreenPass();
    void CreatePreviousTexture();
    void CreateMergeHalfResTexturesPass();

    bool m_isInitialized;

    // Used for half-res rendering
    Handle<FullScreenPass> m_mergeHalfResTexturesPass;
};

} // namespace Hyperion

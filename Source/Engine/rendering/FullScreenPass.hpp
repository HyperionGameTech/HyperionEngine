/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/threading/ThreadSignal.hpp>

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

class FullScreenPass
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    friend struct RecreateFullScreenPassFramebuffer;

    explicit FullScreenPass(EnumFlags<FullScreenPassFlags> flags = FSP_NONE);

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
        const ShaderDesc& shaderDesc,
        TextureFormat imageFormat,
        Vec2u extent,
        GBuffer* gbuffer,
        EnumFlags<FullScreenPassFlags> flags = FSP_NONE);

    FullScreenPass(
        const ShaderDesc& shaderDesc,
        const FramebufferRef& framebuffer,
        TextureFormat imageFormat,
        Vec2u extent,
        GBuffer* gbuffer,
        EnumFlags<FullScreenPassFlags> flags = FSP_NONE);

    FullScreenPass(const FullScreenPass&) = delete;
    FullScreenPass& operator=(const FullScreenPass&) = delete;

    virtual ~FullScreenPass();

    virtual Name GetName() const;

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

    HYP_FORCE_INLINE const ShaderDesc& GetShaderdesc() const
    {
        return m_shaderDesc;
    }

    void SetShaderDesc(const ShaderDesc& shaderDesc);

    HYP_FORCE_INLINE const Handle<Mesh>& GetQuadMesh() const
    {
        return m_fullScreenQuad;
    }

    HYP_FORCE_INLINE const BlendFunction& GetBlendFunction() const
    {
        return m_blendFunction;
    }

    /*! \brief Sets the blend function of the render pass.
        Must be set before Create() is called. */
    void SetBlendFunction(const BlendFunction& blendFunction);

    virtual const GpuImageViewRef& GetFinalImageView() const;
    virtual const GpuImageViewRef& GetPreviousFrameColorImageView() const;

    /*! \brief Resizes the full screen pass to the new size.
     *  Callable on any thread, as it enqueues a render command. */
    void Resize(Vec2u newSize);

    virtual void CreateFramebuffer();

    /*! \brief Create the full screen pass */
    virtual void Create();

    virtual void Render(Frame* frame, const RenderSetup& renderSetup);
    void RenderToFramebuffer(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer);
    void RenderFullScreenQuad(Frame* frame, const RenderSetup& renderSetup);

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

    void CreateFullScreenQuad();

    void DrawHistoryTexture(Frame* frame, const RenderSetup& renderSetup);
    void CopyResultToPreviousTexture(Frame* frame, const RenderSetup& renderSetup);
    void MergeHalfResTextures(Frame* frame, const RenderSetup& renderSetup);

    virtual void Resize_Internal(Vec2u newSize);

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer);

    FramebufferRef m_framebuffer;
    Handle<Mesh> m_fullScreenQuad;
    Vec2u m_extent;
    GBuffer* m_gbuffer;

    EnumFlags<FullScreenPassFlags> m_flags;

    TextureFormat m_imageFormat;

    BlendFunction m_blendFunction;

    UniquePtr<TemporalBlending> m_temporalBlending;
    Handle<Texture> m_historyTexture;

    ShaderDesc m_shaderDesc;

    bool m_isFirstFrame;

private:
    void CreateTemporalBlending();
    void CreateHistoryTexture();
    void CreateMergeHalfResTexturesPass();

    bool m_isInitialized;

    // Used for half-res rendering
    UniquePtr<FullScreenPass> m_mergeHalfResTexturesPass;
    GpuBufferRef m_mergeHalfResTexturesUniformBuffer;

    ThreadSignal m_threadSignal; // for render commands
};

} // namespace Hyperion

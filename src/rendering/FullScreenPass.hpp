/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FULL_SCREEN_PASS_HPP
#define HYPERION_FULL_SCREEN_PASS_HPP

#include <rendering/RenderableAttributes.hpp>

#include <rendering/rhi/CmdList.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/Handle.hpp>

#include <Constants.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Mesh;
class Texture;
class RenderGroup;
class TemporalBlending;
class GBuffer;
class RenderView;
struct RenderSetup;

class HYP_API FullScreenPass
{
public:
    friend struct RenderCommand_RecreateFullScreenPassFramebuffer;

    FullScreenPass(
        TextureFormat image_format,
        GBuffer* gbuffer);

    FullScreenPass(
        TextureFormat image_format,
        Vec2u extent,
        GBuffer* gbuffer);

    FullScreenPass(
        const ShaderRef& shader,
        TextureFormat image_format,
        Vec2u extent,
        GBuffer* gbuffer);

    FullScreenPass(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptor_table,
        TextureFormat image_format,
        Vec2u extent,
        GBuffer* gbuffer);

    FullScreenPass(
        const ShaderRef& shader,
        const DescriptorTableRef& descriptor_table,
        const FramebufferRef& framebuffer,
        TextureFormat image_format,
        Vec2u extent,
        GBuffer* gbuffer);

    FullScreenPass(const FullScreenPass&) = delete;
    FullScreenPass& operator=(const FullScreenPass&) = delete;
    virtual ~FullScreenPass();

    HYP_FORCE_INLINE const Vec2u& GetExtent() const
    {
        return m_extent;
    }

    HYP_FORCE_INLINE TextureFormat GetFormat() const
    {
        return m_image_format;
    }

    AttachmentBase* GetAttachment(uint32 attachment_index) const;

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
        return m_full_screen_quad;
    }

    HYP_FORCE_INLINE const GraphicsPipelineRef& GetGraphicsPipeline() const
    {
        return m_graphics_pipeline;
    }

    HYP_FORCE_INLINE void SetPushConstants(const PushConstantData& pc)
    {
        m_push_constant_data = pc;
    }

    HYP_FORCE_INLINE void SetPushConstants(const void* ptr, SizeType size)
    {
        SetPushConstants(PushConstantData(ptr, size));
    }

    HYP_FORCE_INLINE const BlendFunction& GetBlendFunction() const
    {
        return m_blend_function;
    }

    /*! \brief Sets the blend function of the render pass.
        Must be set before Create() is called. */
    void SetBlendFunction(const BlendFunction& blend_function);

    HYP_FORCE_INLINE const Optional<DescriptorTableRef>& GetDescriptorTable() const
    {
        return m_descriptor_table;
    }

    virtual const ImageViewRef& GetFinalImageView() const;
    virtual const ImageViewRef& GetPreviousFrameColorImageView() const;

    /*! \brief Resizes the full screen pass to the new size.
     *  Callable on any thread, as it enqueues a render command. */
    void Resize(Vec2u new_size);

    virtual void CreateFramebuffer();
    virtual void CreatePipeline(const RenderableAttributeSet& renderable_attributes);
    virtual void CreatePipeline();
    virtual void CreateDescriptors();

    /*! \brief Create the full screen pass */
    virtual void Create();

    virtual void Render(FrameBase* frame, const RenderSetup& render_setup);
    virtual void RenderToFramebuffer(FrameBase* frame, const RenderSetup& render_setup, const FramebufferRef& framebuffer);

    void Begin(FrameBase* frame, const RenderSetup& render_setup);
    void End(FrameBase* frame, const RenderSetup& render_setup);

protected:
    virtual bool UsesTemporalBlending() const
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u new_size);

    void CreateQuad();

    void RenderPreviousTextureToScreen(FrameBase* frame, const RenderSetup& render_setup);
    void CopyResultToPreviousTexture(FrameBase* frame, const RenderSetup& render_setup);
    void MergeHalfResTextures(FrameBase* frame, const RenderSetup& render_setup);

    FramebufferRef m_framebuffer;
    ShaderRef m_shader;
    GraphicsPipelineRef m_graphics_pipeline;
    Handle<Mesh> m_full_screen_quad;
    Vec2u m_extent;
    GBuffer* m_gbuffer;

    PushConstantData m_push_constant_data;

    TextureFormat m_image_format;

    BlendFunction m_blend_function;

    Optional<DescriptorTableRef> m_descriptor_table;

    UniquePtr<TemporalBlending> m_temporal_blending;
    Handle<Texture> m_previous_texture;

    UniquePtr<FullScreenPass> m_render_texture_to_screen_pass;

    bool m_is_first_frame;

private:
    void CreateTemporalBlending();
    void CreateRenderTextureToScreenPass();
    void CreatePreviousTexture();
    void CreateMergeHalfResTexturesPass();

    bool m_is_initialized;

    // Used for half-res rendering
    UniquePtr<FullScreenPass> m_merge_half_res_textures_pass;
};

} // namespace hyperion

#endif

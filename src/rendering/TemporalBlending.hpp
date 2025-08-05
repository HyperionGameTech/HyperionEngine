/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Constants.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/ShaderManager.hpp>

#include <rendering/RenderObject.hpp>

namespace hyperion {

class GBuffer;
class Texture;
struct RenderSetup;

enum class TemporalBlendTechnique
{
    TECHNIQUE_0,
    TECHNIQUE_1,
    TECHNIQUE_2,
    TECHNIQUE_3,

    TECHNIQUE_4 // Progressive blending for path tracing
};

enum class TemporalBlendFeedback
{
    LOW,
    MEDIUM,
    HIGH
};

class TemporalBlending
{
public:
    friend struct RenderCommand_RecreateTemporalBlendingFramebuffer;

    TemporalBlending(
        const Vec2u& extent,
        TemporalBlendTechnique technique,
        TemporalBlendFeedback feedback,
        const ImageViewRef& inputImageView,
        GBuffer* gbuffer);

    TemporalBlending(
        const Vec2u& extent,
        TextureFormat imageFormat,
        TemporalBlendTechnique technique,
        TemporalBlendFeedback feedback,
        const FramebufferRef& inputFramebuffer,
        GBuffer* gbuffer);

    TemporalBlending(
        const Vec2u& extent,
        TextureFormat imageFormat,
        TemporalBlendTechnique technique,
        TemporalBlendFeedback feedback,
        const ImageViewRef& inputImageView,
        GBuffer* gbuffer);

    TemporalBlending(const TemporalBlending& other) = delete;
    TemporalBlending& operator=(const TemporalBlending& other) = delete;
    ~TemporalBlending();

    HYP_FORCE_INLINE TemporalBlendTechnique GetTechnique() const
    {
        return m_technique;
    }

    HYP_FORCE_INLINE TemporalBlendFeedback GetFeedback() const
    {
        return m_feedback;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetResultTexture() const
    {
        return m_resultTexture;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetHistoryTexture() const
    {
        return m_historyTexture;
    }

    void ResetProgressiveBlending();

    void Create();
    void Render(FrameBase* frame, const RenderSetup& renderSetup);

    void Resize(Vec2u newSize);

private:
    void Resize_Internal(Vec2u newSize);

    ShaderProperties GetShaderProperties() const;

    void CreateImages();
    void CreatePipeline();

    Vec2u m_extent;
    TextureFormat m_imageFormat;
    TemporalBlendTechnique m_technique;
    TemporalBlendFeedback m_feedback;
    GBuffer* m_gbuffer;

    uint16 m_blendingFrameCounter;

    ComputePipelineRef m_csPerformBlending;
    
    FixedArray<GpuBufferRef, g_framesInFlight> m_uniformBuffers;

    ImageViewRef m_inputImageView;
    FramebufferRef m_inputFramebuffer;

    Handle<Texture> m_resultTexture;
    Handle<Texture> m_historyTexture;

    DelegateHandler m_onGbufferResolutionChanged;

    bool m_isInitialized;
};

} // namespace hyperion

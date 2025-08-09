/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/Scene.hpp>

#include <core/math/Vector2.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/RenderObject.hpp>

namespace hyperion {

class GBuffer;
class Texture;
struct RenderSetup;

struct RenderCommand_CreateTemporalAADescriptors;
struct RenderCommand_DestroyTemporalAADescriptorsAndImageOutputs;
struct RenderCommand_CreateTemporalAAImageOutputs;

class TemporalAA
{
public:
    TemporalAA(const GpuImageViewRef& inputImageView, const Vec2u& extent, GBuffer* gbuffer);
    TemporalAA(const TemporalAA& other) = delete;
    TemporalAA& operator=(const TemporalAA& other) = delete;
    ~TemporalAA();

    HYP_FORCE_INLINE const GpuImageViewRef& GetInputImageView() const
    {
        return m_inputImageView;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetResultTexture() const
    {
        return m_resultTexture;
    }

    void Create();
    void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    void CreateTextures();

    void UpdatePipelineState(FrameBase* frame, const RenderSetup& renderSetup);

    Vec2u m_extent;

    GpuImageViewRef m_inputImageView;
    GpuBufferRef m_uniformBuffer;
    GBuffer* m_gbuffer;

    Handle<Texture> m_resultTexture;
    Handle<Texture> m_historyTexture;

    ComputePipelineRef m_computePipeline;

    DelegateHandler m_onGbufferResolutionChanged;

    bool m_isInitialized;
};

} // namespace hyperion

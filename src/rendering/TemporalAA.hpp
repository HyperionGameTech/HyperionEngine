/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEMPORAL_AA_HPP
#define HYPERION_TEMPORAL_AA_HPP

#include <scene/Scene.hpp>

#include <core/math/Vector2.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class Engine;
class GBuffer;
class ViewRenderResource;

struct RenderCommand_CreateTemporalAADescriptors;
struct RenderCommand_DestroyTemporalAADescriptorsAndImageOutputs;
struct RenderCommand_CreateTemporalAAImageOutputs;

class TemporalAA
{
public:
    friend struct RenderCommand_CreateTemporalAADescriptors;
    friend struct RenderCommand_DestroyTemporalAADescriptorsAndImageOutputs;
    friend struct RenderCommand_CreateTemporalAAImageOutputs;

    TemporalAA(const Vec2u &extent, const ImageViewRef &input_image_view, GBuffer *gbuffer);
    TemporalAA(const TemporalAA &other)             = delete;
    TemporalAA &operator=(const TemporalAA &other)  = delete;
    ~TemporalAA();

    HYP_FORCE_INLINE const ImageViewRef &GetInputImageView() const
        { return m_input_image_view; }

    HYP_FORCE_INLINE const Handle<Texture> &GetResultTexture() const
        { return m_result_texture; }

    void Resize(Vec2u resolution);

    void Create();
    void Render(FrameBase *frame, ViewRenderResource *view);

private:
    void CreateImages();
    void CreateDescriptorSets();
    void CreateComputePipelines();

    Vec2u               m_extent;

    ImageViewRef        m_input_image_view;
    GBuffer             *m_gbuffer;

    Handle<Texture>     m_result_texture;
    Handle<Texture>     m_history_texture;

    ComputePipelineRef  m_compute_taa;

    DelegateHandler     m_on_gbuffer_resolution_changed;

    bool                m_is_initialized;
};

} // namespace hyperion

#endif
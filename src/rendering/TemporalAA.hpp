/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEMPORAL_AA_HPP
#define HYPERION_TEMPORAL_AA_HPP

#include <scene/Scene.hpp>

#include <core/math/Vector2.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class Engine;

struct RenderCommand_CreateTemporalAADescriptors;
struct RenderCommand_DestroyTemporalAADescriptorsAndImageOutputs;
struct RenderCommand_CreateTemporalAAImageOutputs;

class TemporalAA
{
public:
    friend struct RenderCommand_CreateTemporalAADescriptors;
    friend struct RenderCommand_DestroyTemporalAADescriptorsAndImageOutputs;
    friend struct RenderCommand_CreateTemporalAAImageOutputs;

    TemporalAA(const Vec2u &extent);
    TemporalAA(const TemporalAA &other) = delete;
    TemporalAA &operator=(const TemporalAA &other) = delete;
    ~TemporalAA();

    void Resize(Vec2u resolution);

    void Create();
    void Render(FrameBase *frame);

private:
    void CreateImages();
    void CreateDescriptorSets();
    void CreateComputePipelines();

    Vec2u               m_extent;

    Handle<Texture>     m_result_texture;
    Handle<Texture>     m_history_texture;

    ComputePipelineRef  m_compute_taa;

    bool                m_is_initialized;
};

} // namespace hyperion

#endif
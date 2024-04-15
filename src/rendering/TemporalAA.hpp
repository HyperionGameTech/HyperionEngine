/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_V2_TEMPORAL_AA_HPP
#define HYPERION_V2_TEMPORAL_AA_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>

#include <math/Matrix4.hpp>

#include <scene/Scene.hpp>

#include <rendering/RenderState.hpp>
#include <rendering/DrawProxy.hpp>

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

namespace hyperion::v2 {

using renderer::StorageImage;
using renderer::Image;
using renderer::ImageView;
using renderer::Frame;
;
using renderer::Result;

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

    TemporalAA(const Extent2D &extent);
    TemporalAA(const TemporalAA &other) = delete;
    TemporalAA &operator=(const TemporalAA &other) = delete;
    ~TemporalAA();

    void Create();
    void Destroy();
    
    void Render(
        
        Frame *frame
    );

private:
    void CreateImages();
    void CreateDescriptorSets();
    void CreateComputePipelines();

    Extent2D            m_extent;

    Handle<Texture>     m_result_texture;
    Handle<Texture>     m_history_texture;

    ComputePipelineRef  m_compute_taa;
};

} // namespace hyperion::v2

#endif
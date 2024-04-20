/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEMPORAL_AA_HPP
#define HYPERION_TEMPORAL_AA_HPP

#include <scene/Scene.hpp>

#include <rendering/RenderState.hpp>

namespace hyperion {

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

} // namespace hyperion

#endif
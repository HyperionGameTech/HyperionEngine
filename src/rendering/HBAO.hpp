/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HBAO_HPP
#define HYPERION_HBAO_HPP

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>

namespace hyperion {

using renderer::StorageImage;
using renderer::ImageView;
using renderer::Frame;
using renderer::Result;

class Engine;

struct RenderCommand_AddHBAOFinalImagesToGlobalDescriptorSet;

class HBAO
{
public:
    friend struct RenderCommand_AddHBAOFinalImagesToGlobalDescriptorSet;

    HBAO(const Extent2D &extent);
    HBAO(const HBAO &other)             = delete;
    HBAO &operator=(const HBAO &other)  = delete;
    ~HBAO();

    void Create();
    void Destroy();
    
    void Render(Frame *frame);

private:
    void CreatePass();
    void CreateTemporalBlending();

    Extent2D                    m_extent;

    UniquePtr<FullScreenPass>   m_hbao_pass;
    UniquePtr<TemporalBlending> m_temporal_blending;
};

} // namespace hyperion

#endif
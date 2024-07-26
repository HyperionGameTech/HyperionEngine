/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HBAO_HPP
#define HYPERION_HBAO_HPP

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>

#include <core/functional/Delegate.hpp>

namespace hyperion {

using renderer::Result;

class Engine;

struct RenderCommand_AddHBAOFinalImagesToGlobalDescriptorSet;

class HBAO : public FullScreenPass
{
public:
    friend struct RenderCommand_AddHBAOFinalImagesToGlobalDescriptorSet;

    HBAO();
    HBAO(const HBAO &other)             = delete;
    HBAO &operator=(const HBAO &other)  = delete;
    virtual ~HBAO() override;

    virtual void Create() override;

    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

protected:
    virtual void CreatePipeline(const RenderableAttributeSet &renderable_attributes) override;

    virtual void Resize_Internal(Extent2D new_size) override;

private:
    void CreateTemporalBlending();

    UniquePtr<TemporalBlending> m_temporal_blending;
};

} // namespace hyperion

#endif
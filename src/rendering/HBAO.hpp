/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HBAO_HPP
#define HYPERION_HBAO_HPP

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/config/Config.hpp>

#include <core/object/HypObject.hpp>

#include <core/functional/Delegate.hpp>

namespace hyperion {

HYP_STRUCT(ConfigName = "app", JSONPath = "rendering.hbao")

struct HBAOConfig : public ConfigBase<HBAOConfig>
{
    HYP_FIELD()
    float radius = 2.5f;

    HYP_FIELD()
    float power = 0.8f;

    HYP_FIELD()
    bool use_temporal_blending = false;

    virtual ~HBAOConfig() override = default;

    bool Validate() const
    {
        return radius > 0.0f
            && power > 0.0f;
    }
};

class HBAO final : public FullScreenPass
{
public:
    HBAO(HBAOConfig&& config, GBuffer* gbuffer);
    HBAO(const HBAO& other) = delete;
    HBAO& operator=(const HBAO& other) = delete;
    virtual ~HBAO() override;

    virtual void Create() override;

    virtual void Render(FrameBase* frame, RenderView* view) override;

    virtual void RenderToFramebuffer(FrameBase* frame, RenderView* view, const FramebufferRef& framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

protected:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    } // m_config.use_temporal_blending; }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void CreateDescriptors() override;
    virtual void CreatePipeline(const RenderableAttributeSet& renderable_attributes) override;

    virtual void Resize_Internal(Vec2u new_size) override;

private:
    void CreateUniformBuffers();

    HBAOConfig m_config;

    GPUBufferRef m_uniform_buffer;
};

} // namespace hyperion

#endif
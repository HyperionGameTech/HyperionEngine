/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderObject.hpp>

#include <Core/config/Config.hpp>

namespace Hyperion {

HYP_STRUCT(ConfigName = "EngineConfig", JsonPath = "Rendering.HBAO")
struct HBAOConfig : public Config<HBAOConfig>
{
    HYP_STRUCT_BODY(HBAOConfig);

    HYP_FIELD()
    float radius = 2.5f;

    HYP_FIELD()
    float power = 0.8f;

    HYP_FIELD()
    bool useTemporalBlending = false;

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
    HBAO(HBAOConfig&& config, Vec2u extent, GBuffer* gbuffer);
    HBAO(const HBAO& other) = delete;
    HBAO& operator=(const HBAO& other) = delete;
    virtual ~HBAO() override;

    virtual void Render(Frame* frame, const RenderSetup& renderSetup) override;

protected:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return true;
    }

    virtual void Resize_Internal(Vec2u newSize) override;

private:
    HBAOConfig m_config;

    DescriptorSetRef m_descriptorSet;
    GpuBufferRef m_cBuffer;
};

} // namespace Hyperion

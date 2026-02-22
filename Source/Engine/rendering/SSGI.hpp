/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderObject.hpp>

#include <Core/config/Config.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/utilities/EnumFlags.hpp>

namespace Hyperion {

class GBuffer;
class View;
struct SSGIUniforms;

HYP_STRUCT(ConfigName = "GlobalConfig", JsonPath = "Rendering.SSGI")
struct SSGIConfig : public ConfigBase<SSGIConfig>
{
    HYP_STRUCT_BODY(SSGIConfig);

    HYP_FIELD(Description = "The quality level of the SSGI effect. (0 = quarter res, 1 = half res)")
    int quality = 0;

    HYP_FIELD(JsonIgnore)
    Vec2u extent;

    virtual ~SSGIConfig() override = default;

    void PostLoadCallback()
    {
        extent = Vec2u { 1280, 720 };

        switch (quality)
        {
        case 0:
            extent /= 2;
            break;
        default:
            break;
        }
    }
};

class SSGI
{
public:
    SSGI(SSGIConfig&& config, GBuffer* gbuffer);
    ~SSGI();

    HYP_FORCE_INLINE const Handle<Texture>& GetResultTexture() const
    {
        return m_resultTexture;
    }

    const Handle<Texture>& GetFinalResultTexture() const;

    HYP_FORCE_INLINE bool IsRendered() const
    {
        return m_isRendered;
    }

    void Create();

    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    ShaderPropertySet GetShaderProperties() const;

    void CreateUniformBuffers();

    void FillUniformBufferData(View* view, SSGIUniforms& outUniforms) const;

    SSGIConfig m_config;

    GBuffer* m_gbuffer;

    Handle<Texture> m_resultTexture;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_uniformBuffers;

    UniquePtr<TemporalBlending> m_temporalBlending;

    bool m_isRendered;
};

} // namespace Hyperion

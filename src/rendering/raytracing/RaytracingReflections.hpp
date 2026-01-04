/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Constants.hpp>

#include <rendering/TemporalBlending.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>
#include <rendering/raytracing/RenderRaytracingPipeline.hpp>

#include <core/config/Config.hpp>

namespace Hyperion {

class GBuffer;
class PassData;

HYP_STRUCT(ConfigName = "GlobalConfig", JsonPath = "Rendering.RayTracing")
struct RaytracingReflectionsConfig : public ConfigBase<RaytracingReflectionsConfig>
{
    HYP_STRUCT_BODY(RaytracingReflectionsConfig);

    HYP_FIELD(JsonIgnore)
    Vec2u extent = { 1280, 720 };

    HYP_FIELD(JsonPath = "PathTracing.Enabled")
    bool pathTracing = false;

    virtual ~RaytracingReflectionsConfig() override = default;

    bool Validate() const
    {
        return extent.x * extent.y != 0;
    }
};

class RaytracingReflections
{
public:
    friend struct DestroyRaytracingReflections;
    friend struct CreateRTRadianceImageOutputs;

    HYP_API RaytracingReflections(
        RaytracingReflectionsConfig&& config,
        GBuffer* gbuffer);

    HYP_API ~RaytracingReflections();

    HYP_FORCE_INLINE bool IsPathTracer() const
    {
        return m_config.pathTracing;
    }

    HYP_API void Create();

    HYP_API void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    void CreateImages();
    void CreateTemporalBlending();

    void UpdatePipelineState(Frame* frame, const RenderSetup& renderSetup);
    void UpdateUniforms(Frame* frame, const RenderSetup& renderSetup);

    RaytracingReflectionsConfig m_config;

    GBuffer* m_gbuffer;

    Handle<Texture> m_texture;
    UniquePtr<TemporalBlending> m_temporalBlending;

    RaytracingPipelineRef m_raytracingPipeline;

    Mat4f m_previousViewMatrix;
};

} // namespace Hyperion

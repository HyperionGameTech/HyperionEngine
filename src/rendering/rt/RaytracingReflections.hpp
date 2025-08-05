/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Constants.hpp>

#include <rendering/ShaderManager.hpp>
#include <rendering/TemporalBlending.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>
#include <rendering/rt/RenderRaytracingPipeline.hpp>

#include <core/config/Config.hpp>

namespace hyperion {

class Engine;
class GBuffer;
class PassData;

struct RenderCommand_DestroyRaytracingReflections;
struct RenderCommand_CreateRTRadianceImageOutputs;

HYP_STRUCT(ConfigName = "app", JsonPath = "rendering.raytracing")
struct RaytracingReflectionsConfig : public ConfigBase<RaytracingReflectionsConfig>
{
    HYP_FIELD(JsonIgnore)
    Vec2u extent = { 1280, 720 };

    HYP_FIELD(JsonPath = "pathTracing.enabled")
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
    friend struct RenderCommand_DestroyRaytracingReflections;
    friend struct RenderCommand_CreateRTRadianceImageOutputs;

    HYP_API RaytracingReflections(
        RaytracingReflectionsConfig&& config,
        GBuffer* gbuffer);

    HYP_API ~RaytracingReflections();

    HYP_FORCE_INLINE bool IsPathTracer() const
    {
        return m_config.pathTracing;
    }

    HYP_API void Create();

    HYP_API void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    void CreateImages();
    void CreateUniformBuffer();
    void CreateRaytracingPipeline();
    void CreateTemporalBlending();

    void UpdatePipelineState(FrameBase* frame, const RenderSetup& renderSetup);
    void UpdateUniforms(FrameBase* frame, const RenderSetup& renderSetup);

    RaytracingReflectionsConfig m_config;

    GBuffer* m_gbuffer;

    Handle<Texture> m_texture;
    UniquePtr<TemporalBlending> m_temporalBlending;

    RaytracingPipelineRef m_raytracingPipeline;
    FixedArray<GpuBufferRef, g_framesInFlight> m_uniformBuffers;

    Matrix4 m_previousViewMatrix;
};

} // namespace hyperion

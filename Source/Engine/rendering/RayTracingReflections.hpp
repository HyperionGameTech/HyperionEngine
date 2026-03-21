/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Constants.hpp>

#include <rendering/TemporalBlending.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <Core/config/Config.hpp>

namespace Hyperion {

class GBuffer;
class PassData;

HYP_STRUCT(ConfigName = "EngineConfig", JsonPath = "Rendering.RayTracing")
struct RayTracingReflectionsConfig : public Config<RayTracingReflectionsConfig>
{
    HYP_STRUCT_BODY(RayTracingReflectionsConfig);

    HYP_FIELD(JsonIgnore)
    Vec2u extent = { 1280, 720 };

    HYP_FIELD(JsonPath = "PathTracing.Enabled")
    bool pathTracing = false;

    virtual ~RayTracingReflectionsConfig() override = default;

    bool Validate() const
    {
        return extent.x * extent.y != 0;
    }
};

class RayTracingReflections
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    friend struct DestroyRayTracingReflections;
    friend struct CreateRTRadianceImageOutputs;

    RayTracingReflections(
        RayTracingReflectionsConfig&& config,
        GBuffer* gbuffer);

    ~RayTracingReflections();

    HYP_FORCE_INLINE bool IsPathTracer() const
    {
        return m_config.pathTracing;
    }

    const GpuImageViewRef& GetFinalImageView() const;

    void Create();
    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    void CreateImages();
    void CreateTemporalBlending();

    RayTracingReflectionsConfig m_config;

    GBuffer* m_gbuffer;

    Handle<Texture> m_texture;
    UniquePtr<TemporalBlending> m_temporalBlending;

    Mat4f m_previousViewMatrix;
};

} // namespace Hyperion

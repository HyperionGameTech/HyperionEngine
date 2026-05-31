/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Constants.hpp>

#include <Rendering/TemporalBlending.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Shared.hpp>

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

    const GpuImageViewRef& GetFinalImageView() const;

    void Create();
    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    void CreateImages();

    void InitTemporalBlending(bool isPathTracer);

    RayTracingReflectionsConfig m_config;

    GBuffer* m_gbuffer;

    Handle<Texture> m_texture;
    UniquePtr<TemporalBlending> m_temporalBlending;

    Mat4f m_previousViewMatrix;
};

} // namespace Hyperion

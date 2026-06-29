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

#include <Core/Config/Config.hpp>

namespace Hyperion {

class GBuffer;
class PassData;

class RayTracingReflections
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    friend struct DestroyRayTracingReflections;
    friend struct CreateRTRadianceImageOutputs;

    explicit RayTracingReflections(GBuffer* gbuffer);

    RayTracingReflections(const RayTracingReflections& other) = delete;
    RayTracingReflections& operator=(const RayTracingReflections& other) = delete;

    RayTracingReflections(RayTracingReflections&& other) noexcept = delete;
    RayTracingReflections& operator=(RayTracingReflections&& other) noexcept = delete;

    ~RayTracingReflections();

    const GpuImageViewRef& GetFinalImageView() const;

    void Create();
    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    void CreateImages();

    void InitTemporalBlending(bool isPathTracer);

    GBuffer* m_gbuffer;

    Handle<Texture> m_texture;
    UniquePtr<TemporalBlending> m_temporalBlending;

    Mat4f m_previousViewMatrix;
};

} // namespace Hyperion

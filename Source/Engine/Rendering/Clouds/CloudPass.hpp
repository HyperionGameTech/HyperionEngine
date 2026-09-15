/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Mat4f.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class GBuffer;
class Texture;
class Mesh;
struct RenderSetup;
struct RenderProxyCamera;

/*! \brief Per view volumetric clouds. Each frame traces one pixel of every 2x2 half resolution block (quarter resolution),
 *  reconstructs the full half resolution image from last frame's reprojected result, then composites it over sky pixels.
 *  Shared resources (weather map, noise) come from RI.cloudResources. */
class CloudPass
{
public:
    // a view change bigger than this makes last frame's clouds useless to reproject
    static constexpr float CameraCutMinDirectionDot = 0.9f;
    static constexpr float CameraCutMaxMoveDistance = 50.0f;

    CloudPass(const Vec2u& extent, GBuffer* gbuffer);

    CloudPass(const CloudPass&) = delete;
    CloudPass& operator=(const CloudPass&) = delete;

    ~CloudPass();

    void Create();

    /*! \brief Traces and reconstructs renderSetup.view's clouds. renderSetup.passData must be the view's DeferredPassData. */
    void Render(Frame* frame, const RenderSetup& renderSetup);

    /*! \brief Blends the reconstructed clouds over the sky pixels of renderSetup.framebuffer. No-op if Render() didn't run this frame. */
    void Composite(Frame* frame, const RenderSetup& renderSetup);

private:
    void Reconstruct(Frame* frame, const RenderProxyCamera& cameraProxy, bool isHistoryValid, const Vec2u& traceOffset);

    Vec2u m_extent;
    Vec2u m_historyExtent;
    Vec2u m_traceExtent;

    GBuffer* m_gbuffer;

    // rgb = premultiplied cloud light, a = transmittance
    Handle<Texture> m_traceTexture;

    // transmittance weighted distance to the clouds along each ray, used to reproject
    Handle<Texture> m_traceDistanceTexture;

    // ping-ponged half resolution reconstruction; m_historyIndex is the most recently written
    Handle<Texture> m_historyTextures[2];
    Handle<Texture> m_historyDistanceTextures[2];
    uint32 m_historyIndex;

    Mat4f m_previousViewProjection;
    Vec3f m_previousCameraPosition;
    Vec3f m_previousCameraDirection;

    uint32 m_lastRenderedFrame;

    Handle<Mesh> m_quadMesh;

    bool m_hasRenderedThisFrame;
};

} // namespace Hyperion

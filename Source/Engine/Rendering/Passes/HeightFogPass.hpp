/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class Mesh;
struct RenderSetup;

/*! \brief A GBuffer view's exponential height fog and aerial perspective, owned by its DeferredPassData.
 *  Blends over the lit opaque pixels of renderSetup.framebuffer, fading them toward the sky probe in their direction. */
class HeightFogPass
{
public:
    HeightFogPass();

    HeightFogPass(const HeightFogPass&) = delete;
    HeightFogPass& operator=(const HeightFogPass&) = delete;

    ~HeightFogPass();

    void Create();

    /*! \brief No-op unless the world environment has height fog enabled. renderSetup.passData must be the view's DeferredPassData,
     *  and renderSetup.framebuffer must use the view's GBuffer depth for stencil. */
    void Render(Frame* frame, const RenderSetup& renderSetup);

private:
    Handle<Mesh> m_quadMesh;
};

} // namespace Hyperion

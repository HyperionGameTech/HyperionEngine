/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/Pass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Core/Math/Vector2.hpp>

namespace Hyperion {

class View;
class Mesh;
struct RenderSetup;

HYP_CLASS(NoScriptBindings)
class DecalPassData : public PassData
{
    HYP_OBJECT_BODY(DecalPassData);

public:
    virtual ~DecalPassData() override;

    // gbuffer albedo (shared), normal accumulation, copy of the base normals under decals + stencil
    FramebufferRef applyFramebuffer;
    // gbuffer normals (shared) + stencil
    FramebufferRef resolveFramebuffer;

    Vec2u extent;
    Framebuffer* sourceFramebuffer = nullptr;
};

class DecalPass final : public PassBase
{
public:
    DecalPass();
    virtual ~DecalPass() override = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

protected:
    PassData* CreateViewPassData(View* view, PassDataExt&) override;

private:
    void UpdateFramebuffers(View* view, DecalPassData& passData);

    Handle<Mesh> m_cubeMesh;
    Handle<Mesh> m_quadMesh;
};

} // namespace Hyperion

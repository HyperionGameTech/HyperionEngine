/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Resource/Resource.hpp>

#include <Rendering/Pass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Scene/Subsystem.hpp>

namespace Hyperion {

class View;
struct RenderSetup;

HYP_CLASS(NoScriptBindings)
class SpritePassData : public PassData
{
    HYP_OBJECT_BODY(SpritePassData);

public:
    virtual ~SpritePassData() override = default;
};

class SpritePass final : public PassBase
{
public:
    SpritePass();
    virtual ~SpritePass() = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

protected:
    void CreateQuadMeshes();

    PassData* CreateViewPassData(View* view, PassDataExt&) override;

    Handle<Mesh> m_quadMesh;
    Handle<Mesh> m_textQuadFrontMesh;
    Handle<Mesh> m_textQuadBackMesh;
};

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/memory/resource/Resource.hpp>

#include <rendering/RendererBase.hpp>
#include <rendering/RenderObject.hpp>

#include <scene/Subsystem.hpp>

namespace Hyperion {

class View;
struct RenderSetup;

HYP_CLASS(NoScriptBindings)
class HYP_API SpriteRendererPassData : public PassData
{
    HYP_OBJECT_BODY(SpriteRendererPassData);

public:
    virtual ~SpriteRendererPassData() override = default;
};

class SpriteRenderer final : public RendererBase
{
public:
    SpriteRenderer();
    virtual ~SpriteRenderer() = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

protected:
    PassData* CreateViewPassData(View* view, PassDataExt&) override;

    Handle<Mesh> m_quadMesh;
};

} // namespace Hyperion
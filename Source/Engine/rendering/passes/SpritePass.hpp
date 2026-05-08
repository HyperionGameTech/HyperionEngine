/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/memory/resource/Resource.hpp>

#include <rendering/Pass.hpp>
#include <rendering/RenderObject.hpp>

#include <scene/Subsystem.hpp>

namespace Hyperion {

class View;
struct RenderSetup;

HYP_CLASS(NoScriptBindings)
class HYP_API SpritePassData : public PassData
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
    PassData* CreateViewPassData(View* view, PassDataExt&) override;

    Handle<Mesh> m_quadMesh;
};

} // namespace Hyperion

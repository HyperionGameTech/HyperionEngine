/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/resource/Resource.hpp>

#include <rendering/Pass.hpp>
#include <rendering/RendererMain.hpp>
#include <rendering/RenderTypes.hpp>

#include <scene/Scene.hpp>
#include <scene/Subsystem.hpp>

namespace Hyperion {

class UIStage;
class UIObject;
class View;
struct RenderSetup;

class UIRenderCollector : public RenderCollector
{
public:
    UIRenderCollector() = default;
    ~UIRenderCollector() = default;

    void ExecuteDrawCalls(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer, uint32 bucketBits);
};

HYP_CLASS(NoScriptBindings)
class UIPassData : public PassData
{
    HYP_OBJECT_BODY(UIPassData);

public:
    virtual ~UIPassData() override = default;

    UIRenderCollector renderCollector;
};

class UIPass final : public PassBase
{
public:
    UIPass();
    virtual ~UIPass() = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

protected:
    PassData* CreateViewPassData(View* view, PassDataExt&) override;
};

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/resource/Resource.hpp>

#include <rendering/RendererBase.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderObject.hpp>

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
class HYP_API UIRendererPassData : public PassData
{
    HYP_OBJECT_BODY(UIRendererPassData);

public:
    virtual ~UIRendererPassData() override = default;

    UIRenderCollector renderCollector;
};

class UIRenderer final : public RendererBase
{
public:
    UIRenderer();
    virtual ~UIRenderer() = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

protected:
    PassData* CreateViewPassData(View* view, PassDataExt&) override;
};

} // namespace Hyperion

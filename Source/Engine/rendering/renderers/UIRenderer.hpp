/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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

HYP_CLASS(NoScriptBindings)
class HYP_API UIRendererPassData : public PassData
{
    HYP_OBJECT_BODY(UIRendererPassData);

public:
    virtual ~UIRendererPassData() override = default;
};

class UIRenderCollector : public RenderCollector
{
public:
    UIRenderCollector() = default;
    ~UIRenderCollector() = default;

    void ExecuteDrawCalls(Frame* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer, uint32 bucketBits);
};

class UIRenderer : public RendererBase
{
public:
    UIRenderer(const Handle<View>& view);
    virtual ~UIRenderer() = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override;

    UIRenderCollector renderCollector;

protected:
    PassData* CreateViewPassData(View* view, PassDataExt&) override;

    Handle<View> m_view;
};

} // namespace Hyperion

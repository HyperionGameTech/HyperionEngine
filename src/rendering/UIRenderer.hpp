/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/functional/Delegate.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/reflection/HypObject.hpp>

#include <rendering/Renderer.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/RenderObject.hpp>

#include <scene/Scene.hpp>
#include <scene/Subsystem.hpp>

namespace hyperion {

class UIStage;
class UIObject;
class View;
struct RenderSetup;

HYP_CLASS(NoScriptBindings)
class HYP_API UIPassData : public PassData
{
    HYP_OBJECT_BODY(UIPassData);

public:
    virtual ~UIPassData() override = default;
};

class UIRenderCollector : public RenderCollector
{
public:
    UIRenderCollector() = default;
    ~UIRenderCollector() = default;

    void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer, uint32 bucketBits);
};

class UIRenderer : public RendererBase
{
public:
    UIRenderer(const Handle<View>& view);
    virtual ~UIRenderer() = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) override;

    UIRenderCollector renderCollector;

protected:
    Handle<PassData> CreateViewPassData(View* view, PassDataExt&) override;

    Handle<View> m_view;
};

} // namespace hyperion

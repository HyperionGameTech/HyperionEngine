/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/Subsystem.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/functional/Delegate.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/DrawCall.hpp>

namespace Hyperion {

class Scene;
class View;
class World;

class UIStage;
class UIRenderer;

class RenderProxyList;

HYP_STRUCT(NoScriptBindings)
struct UIEntityInstanceBatch : EntityInstanceBatch
{
    HYP_STRUCT_BODY(UIEntityInstanceBatch);

    HYP_FIELD()
    FixedArray<Vec4f, MaxEntitiesPerBatch> texcoords;

    HYP_FIELD()
    FixedArray<Vec4f, MaxEntitiesPerBatch> offsets;

    HYP_FIELD()
    FixedArray<Vec4f, MaxEntitiesPerBatch> sizes;

    HYP_FIELD()
    FixedArray<Vec4u, MaxEntitiesPerBatch> properties;
};

HYP_CLASS(NoScriptBindings)
class HYP_API UISubsystem final : public Subsystem
{
    HYP_OBJECT_BODY(UISubsystem);

public:
    UISubsystem();
    explicit UISubsystem(const Handle<UIStage>& uiStage);

    UISubsystem(const UISubsystem& other) = delete;
    UISubsystem& operator=(const UISubsystem& other) = delete;

    virtual ~UISubsystem();

    HYP_FORCE_INLINE const Handle<UIStage>& GetUIStage() const
    {
        return m_uiStage;
    }

    virtual void PreUpdate(float delta) override;
    virtual void Update(float delta) override;

protected:
    virtual SubsystemUpdatePhase GetUpdatePhase_Internal() const
    {
        return SubsystemUpdatePhase::AfterVis;
    }

private:
    virtual void Init() override;

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;

    void CreateFramebuffer();

    void RenderCollect(RenderProxyList& rpl);

    Handle<UIStage> m_uiStage;

    ShaderInstanceRef m_shader;

    Handle<View> m_view;

    UIRenderer* m_uiRenderer;

    DelegateHandler m_onWindowResizedHandle;
    DelegateHandler m_onCurrentWindowChangedHandle;
};

} // namespace Hyperion

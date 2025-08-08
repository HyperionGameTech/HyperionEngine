/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/Subsystem.hpp>

#include <core/object/Handle.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/RenderObject.hpp>

namespace hyperion {

class Scene;
class View;
class World;

class UIStage;
class UIRenderer;

HYP_CLASS(NoScriptBindings)
class HYP_API UISubsystem : public Subsystem
{
    HYP_OBJECT_BODY(UISubsystem);

public:
    UISubsystem(const Handle<UIStage>& uiStage);
    UISubsystem(const UISubsystem& other) = delete;
    UISubsystem& operator=(const UISubsystem& other) = delete;
    virtual ~UISubsystem();

    HYP_FORCE_INLINE const Handle<UIStage>& GetUIStage() const
    {
        return m_uiStage;
    }

    virtual void PreUpdate(float delta) override;
    virtual void Update(float delta) override;

private:
    virtual void Init() override;

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;

    void CreateFramebuffer();

    Handle<UIStage> m_uiStage;

    ShaderRef m_shader;

    Handle<View> m_view;

    UIRenderer* m_uiRenderer;

    DelegateHandler m_onResizeHandle;
};

} // namespace hyperion


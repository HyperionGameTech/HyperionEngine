/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_SUBSYSTEM_HPP
#define HYPERION_UI_SUBSYSTEM_HPP

#include <scene/Subsystem.hpp>

#include <core/Handle.hpp>

#include <core/containers/HashMap.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class UIStage;
class UIRenderSubsystem;
class Scene;

HYP_CLASS()
class HYP_API UISubsystem : public Subsystem
{
    HYP_OBJECT_BODY(UISubsystem);

public:
    UISubsystem(const Handle<UIStage>& ui_stage);
    virtual ~UISubsystem() override;

    bool RequiresUpdateOnGameThread() const override
    {
        return false;
    }

    void OnAddedToWorld() override;
    void OnRemovedFromWorld() override;

    void PreUpdate(GameCounter::TickUnit delta) override;
    void Update(GameCounter::TickUnit delta) override;

    void OnSceneAttached(const Handle<Scene>& scene) override;
    void OnSceneDetached(const Handle<Scene>& scene) override;

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<UIStage>& GetUIStage() const
    {
        return m_ui_stage;
    }

private:
    Handle<UIStage> m_ui_stage;
    RC<UIRenderSubsystem> m_ui_render_subsystem;
};

} // namespace hyperion

#endif

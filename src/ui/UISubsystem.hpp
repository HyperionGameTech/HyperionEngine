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
    UISubsystem(const RC<UIStage>& ui_stage);
    virtual ~UISubsystem() override;

    virtual bool RequiresUpdateOnGameThread() const
    {
        return false;
    }

    virtual void Initialize() override;
    virtual void Shutdown() override;
    virtual void PreUpdate(GameCounter::TickUnit delta) override;
    virtual void Update(GameCounter::TickUnit delta) override;

    virtual void OnSceneAttached(const Handle<Scene>& scene) override;
    virtual void OnSceneDetached(const Handle<Scene>& scene) override;

    HYP_METHOD()
    HYP_FORCE_INLINE const RC<UIStage>& GetUIStage() const
    {
        return m_ui_stage;
    }

private:
    RC<UIStage> m_ui_stage;
    RC<UIRenderSubsystem> m_ui_render_subsystem;
};

} // namespace hyperion

#endif

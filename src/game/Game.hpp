/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/Defines.hpp>

namespace hyperion {

class UISubsystem;
class World;
class Scene;

namespace sys {
class SystemEvent;
} // namespace sys

using sys::SystemEvent;

HYP_CLASS(Abstract)
class HYP_API Game : public ObjectBase
{
    friend class GameThread;
    friend class EngineDriver;

    HYP_OBJECT_BODY(Game);

public:
    Game();
    virtual ~Game();

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    virtual void Update(float delta) final;
    virtual void HandleEvent(SystemEvent&& event) final;

protected:
    virtual void Init() override;

    virtual void Logic(float delta) = 0;
    virtual void OnInputEvent(const SystemEvent& event);

    const Handle<UISubsystem>& GetUISubsystem() const
    {
        return m_uiSubsystem;
    }

    Handle<UISubsystem> m_uiSubsystem;

    Handle<World> m_world;
};

} // namespace hyperion

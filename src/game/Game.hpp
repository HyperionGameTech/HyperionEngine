/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <util/GameCounter.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/resource/Resource.hpp>

#include <core/reflection/HypObject.hpp>

#include <core/utilities/Optional.hpp>

#include <core/Defines.hpp>

#include <system/AppContext.hpp>

#include <input/InputManager.hpp>

#include <scene/Scene.hpp>

#include <rendering/RenderObject.hpp>

namespace hyperion {

namespace dotnet {
class Assembly;
class ManagedObject;
} // namespace dotnet

class UISubsystem;
class World;

struct ManagedGameInfo
{
    String assemblyName;
    String className;
};

HYP_CLASS(Abstract)
class HYP_API Game : public HypObjectBase
{
    friend class GameThread;
    friend class EngineDriver;

    HYP_OBJECT_BODY(Game);

public:
    Game();
    Game(Optional<ManagedGameInfo> managedGameInfo);
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

    RC<dotnet::Assembly> m_managedAssembly;
    dotnet::ManagedObject* m_managedGameObject;

    Handle<World> m_world;

private:
    Optional<ManagedGameInfo> m_managedGameInfo;
};

} // namespace hyperion

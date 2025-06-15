/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GAME_HPP
#define HYPERION_GAME_HPP

#include <GameCounter.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/resource/Resource.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/Optional.hpp>

#include <core/Defines.hpp>

#include <system/AppContext.hpp>

#include <input/InputManager.hpp>

#include <scene/Scene.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

namespace dotnet {
class Assembly;
class Object;
} // namespace dotnet

class Engine;
class GameThread;
class UISubsystem;
class RenderScene;

struct ManagedGameInfo
{
    String assembly_name;
    String class_name;
};

HYP_CLASS(Abstract)
class HYP_API Game : public HypObject<Game>
{
    friend class GameThread;
    friend class Engine;

    HYP_OBJECT_BODY(Game);

public:
    Game();
    Game(Optional<ManagedGameInfo> managed_game_info);
    virtual ~Game();

    const Handle<AppContextBase>& GetAppContext() const
    {
        return m_app_context;
    }

    void SetAppContext(const Handle<AppContextBase>& app_context)
    {
        m_app_context = app_context;
    }

    virtual void Update(GameCounter::TickUnit delta) final;
    virtual void HandleEvent(SystemEvent&& event) final;
    
protected:
    virtual void Init() override;
    
    virtual void Logic(GameCounter::TickUnit delta) = 0;
    virtual void OnInputEvent(const SystemEvent& event);

    const RC<UISubsystem>& GetUISubsystem() const
    {
        return m_ui_subsystem;
    }

    Handle<AppContextBase> m_app_context;

    RC<UISubsystem> m_ui_subsystem;

    RC<dotnet::Assembly> m_managed_assembly;
    dotnet::Object* m_managed_game_object;

private:
    Optional<ManagedGameInfo> m_managed_game_info;
};

} // namespace hyperion

#endif
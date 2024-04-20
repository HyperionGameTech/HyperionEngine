/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GAME_HPP
#define HYPERION_GAME_HPP

#include <GameCounter.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/utilities/Optional.hpp>

#include <input/InputManager.hpp>

#include <scene/Scene.hpp>

#include <ui/UIStage.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <system/Application.hpp>

#include <dotnet/Assembly.hpp>
#include <dotnet/Object.hpp>

#include <core/Defines.hpp>

namespace hyperion {

using renderer::Frame;

using dotnet::Object;
using dotnet::Assembly;

class Engine;

struct ManagedGameInfo
{
    String  assembly_name;
    String  class_name;
};

class Game
{
    friend class GameThread;

public:
    HYP_API Game(RC<Application> application);
    HYP_API Game(RC<Application> application, Optional<ManagedGameInfo> managed_game_info);
    HYP_API virtual ~Game();

    const Handle<Scene> &GetScene() const
        { return m_scene; }

    const RC<Application> &GetApplication() const
        { return m_application; }

    HYP_API virtual void Init() final;
    HYP_API virtual void Update(GameCounter::TickUnit delta) final;
    HYP_API virtual void Teardown();

    HYP_API virtual void InitGame();
    HYP_API virtual void InitRender();

    HYP_API virtual void OnFrameBegin(Frame *frame);
    HYP_API virtual void OnFrameEnd(Frame *frame);

    HYP_API virtual void HandleEvent(SystemEvent &&event) final;
    HYP_API virtual void OnInputEvent(const SystemEvent &event);

protected:
    virtual void Logic(GameCounter::TickUnit delta) = 0;

    const RC<UIStage> &GetUIStage() const
        { return m_ui_stage; }

    const UniquePtr<InputManager> &GetInputManager() const
        { return m_input_manager; }

    RC<Application>             m_application;

    UniquePtr<InputManager>     m_input_manager;
    Handle<Scene>               m_scene;

    RC<UIStage>                 m_ui_stage;

    RC<Assembly>                m_managed_assembly;
    UniquePtr<Object>           m_managed_game_object;

private:
    bool                        m_is_init;

    Optional<ManagedGameInfo>   m_managed_game_info;
};

} // namespace hyperion

#endif
#pragma once
#include <core/Defines.hpp>

#include <core/reflection/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

namespace hyperion {

class Game;
class GameThread;

namespace sys {

class AppContextBase;

class HYP_API App final
{
public:
    static App& GetInstance();

    App(const App& other) = delete;
    App& operator=(const App& other) = delete;
    App(App&& other) noexcept = delete;
    App& operator=(App&& other) noexcept = delete;
    virtual ~App();

    const Handle<AppContextBase>& GetAppContext() const
    {
        return m_appContext;
    }

    void LaunchGame(const Handle<Game>& game);

protected:
    App();

private:
    void RunMainLoop(Game* game);

    Handle<AppContextBase> m_appContext;
    UniquePtr<GameThread> m_gameThread;
};

} // namespace sys

using sys::App;

} // namespace hyperion

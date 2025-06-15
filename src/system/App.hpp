#ifndef HYPERION_APP_HPP
#define HYPERION_APP_HPP

#include <core/Defines.hpp>

#include <core/Handle.hpp>

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
        return m_app_context;
    }

    void LaunchGame(const Handle<Game>& game);

protected:
    App();

private:
    void RunMainLoop(Game* game);

    Handle<AppContextBase> m_app_context;
    UniquePtr<GameThread> m_game_thread;
};

} // namespace sys

using sys::App;

} // namespace hyperion

#endif
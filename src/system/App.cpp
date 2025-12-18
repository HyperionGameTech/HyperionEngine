#include <SystemPch.hpp>

#include <system/App.hpp>
#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderBackend.hpp>

#include <game/Game.hpp>

#include <engine/EngineDriver.hpp>

#include <engine/threads/GameThread.hpp>

#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

extern const CommandLineArguments& CoreApi_GetCommandLineArguments();

namespace sys {

App& App::GetInstance()
{
    static App s_instance;
    return s_instance;
}

App::App()
{
}

App::~App()
{
}

} // namespace sys
} // namespace hyperion

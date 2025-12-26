#include <SystemPch.hpp>

#include <system/App.hpp>
#include <system/AppContext.hpp>
#include <input/Event.hpp>

#include <core/cli/CommandLine.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderBackend.hpp>

#include <engine/Game.hpp>

#include <engine/EngineDriver.hpp>

#include <engine/threads/SimThread.hpp>

#include <HyperionEngine.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

extern const CommandLineArguments& CoreApi_GetCommandLineArguments();

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

} // namespace Hyperion

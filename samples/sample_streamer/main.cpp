#include <editor/HyperionEditor.hpp>

#include <system/App.hpp>

#include <core/logging/Logger.hpp>

#include <HyperionEngine.hpp>
#include <engine/EngineDriver.hpp>

#include <game/DefaultGame.hpp>

using namespace hyperion;

int main(int argc, char** argv)
{
    if (!hyperion::InitializeEngine(argc, argv))
    {
        return 1;
    }

    // Handle<HyperionEditor> editorInstance = CreateObject<HyperionEditor>();
    Handle<game::DefaultGame> defaultGame = CreateObject<game::DefaultGame>();

    App::GetInstance().LaunchGame(defaultGame);

    return 0;
}

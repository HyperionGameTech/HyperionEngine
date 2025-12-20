#include <editor/HyperionEditor.hpp>

#include <system/App.hpp>

#include <core/logging/Logger.hpp>

#include <HyperionEngine.hpp>
#include <engine/EngineDriver.hpp>

#include <game/DefaultGame.hpp>

using namespace hyperion;

int main(int argc, char** argv)
{
    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }
    
    Handle<game::DefaultGame> defaultGame = CreateObject<game::DefaultGame>();
    
    Hyp_SetGame(defaultGame);
    Hyp_LaunchThreads();
    Hyp_Shutdown();
    
    return 0;
}

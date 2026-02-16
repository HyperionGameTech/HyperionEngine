#include <editor/HyperionEditor.hpp>

#include <core/logging/Logger.hpp>

#include <HyperionEngine.hpp>

#include <game/DefaultGame.hpp>

using namespace Hyperion;

int main(int argc, char** argv)
{
    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }

    Handle<game::DefaultGame> defaultGame = MakeHandle<game::DefaultGame>();

    Hyp_SetGame(defaultGame);

    if (!Hyp_LaunchThreads())
    {
        HYP_FAIL("Failed to launch threads!");
    }

    Hyp_Shutdown();

    return 0;
}

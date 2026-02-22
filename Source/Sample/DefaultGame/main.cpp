#include <editor/HyperionEditor.hpp>

#include <Core/logging/Logger.hpp>

#include <HyperionEngine.hpp>

#include <game/DefaultGame.hpp>

using namespace Hyperion;

int main(int argc, char** argv)
{
    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }

    game::DefaultGame defaultGame;

    Hyp_SetGame(&defaultGame);

    if (!Hyp_LaunchThreads())
    {
        HYP_FAIL("Failed to launch threads!");
    }

    Hyp_Shutdown();

    return 0;
}

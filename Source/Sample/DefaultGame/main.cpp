#include <Core/Logging/Logger.hpp>

#include <HyperionEngine.hpp>

#include <Game/DefaultGame.hpp>

using namespace Hyperion;

int main(int argc, char** argv)
{
    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }

    auto defaultGame = MakeUnique<game::DefaultGame>();

    Hyp_SetGame(defaultGame.Get());

    if (!Hyp_LaunchThreads())
    {
        HYP_FAIL("Failed to launch threads!");
    }

    Hyp_Shutdown();

    return 0;
}

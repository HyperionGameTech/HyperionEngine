#include <Core/Logging/Logger.hpp>

#include <HyperionEngine.hpp>

#include <Game/DefaultGame.hpp>

using namespace Hyperion;

#ifdef HYP_TESTS
namespace Hyperion {
namespace tests {
namespace profiling {
HYP_IMPORT void PrintContainerProfiling(size_t runsPer = 5, size_t numIterations = 50, size_t runsPerIteration = 10);
} // namespace profiling
} // namespace tests
} // namespace Hyperion
#endif

int main(int argc, char** argv)
{
    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }

    //#ifdef HYP_TESTS
    //    tests::profiling::PrintContainerProfiling(5, 5, 15);
    //    return 0;
    //#endif

    auto defaultGame = MakeUnique<game::DefaultGame>();

    Hyp_SetGame(defaultGame.Get());

    if (!Hyp_LaunchThreads())
    {
        HYP_FAIL("Failed to launch threads!");
    }

    Hyp_Shutdown();

    return 0;
}

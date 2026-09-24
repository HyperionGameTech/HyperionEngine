#include <Core/Logging/Logger.hpp>

#include <Framework/Game.hpp>

#include <HyperionEngine.hpp>

using namespace Hyperion;

#if defined(HYP_TESTS) && defined(HYP_STRATA) && defined(HYP_STRATA_JIT)
namespace Hyperion::tests::script {
    ENGINE_API void RunScriptBenchmark();
    ENGINE_API void RunStrataComponentTests();
}
#endif

#if defined(HYP_TESTS)
namespace Hyperion::tests::scene {
    ENGINE_API void RunComponentContainerTests();
}
#endif

int main(int argc, char** argv)
{
    if (!Hyp_Initialize(argc, argv))
    {
        return 1;
    }

#if defined(HYP_TESTS) && defined(HYP_STRATA) && defined(HYP_STRATA_JIT)
    ///Hyperion::tests::script::RunScriptBenchmark();
    ///Hyperion::tests::script::RunStrataComponentTests();
#endif

#if defined(HYP_TESTS)
    ///Hyperion::tests::scene::RunComponentContainerTests();
#endif

    Handle<Game> defaultGame = Game::CreateGame("DefaultGame"_sh);

    Hyp_SetGame(defaultGame.Get());

    if (!Hyp_LaunchThreads())
    {
        HYP_FAIL("Failed to launch threads!");
    }

    Hyp_Shutdown();

    return 0;
}

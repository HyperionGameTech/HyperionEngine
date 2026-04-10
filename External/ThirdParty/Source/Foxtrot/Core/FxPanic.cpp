#include "FxPanic.hpp"

#include <Asset/AxManager.hpp>
#include <FxEngine.hpp>


void FxTerminate()
{
    gAssetManager->GetInstance()->Shutdown();

    FX_BREAKPOINT;

    std::terminate();
}

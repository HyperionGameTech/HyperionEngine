#include "Ax_Fwd_Manager.hpp"

#include <Asset/AxManager.hpp>
#include <FxEngine.hpp>

namespace Fwd::AssetManager {

FxTSRef<AxImage> LoadImageFromMemory(RxImageFormat format, const uint8* data, uint32 data_size)
{
    return gAssetManager->LoadImageFromMemory(format, data, data_size);
}

} // namespace Fwd::AssetManager

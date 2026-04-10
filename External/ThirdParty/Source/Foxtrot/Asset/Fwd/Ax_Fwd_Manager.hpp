#pragma once

#include <Asset/AxImage.hpp>

namespace Fwd::AssetManager {

FxTSRef<AxImage> LoadImageFromMemory(RxImageFormat format, const uint8* data, uint32 data_size);

}; // namespace Fwd::AssetManager

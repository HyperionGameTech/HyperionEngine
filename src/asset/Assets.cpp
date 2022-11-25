#include "Assets.hpp"
#include <Engine.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

ComponentSystem &AssetManager::GetObjectSystem()
{
    return Engine::Get()->GetObjectSystem();
}

} // namespace hyperion::v2
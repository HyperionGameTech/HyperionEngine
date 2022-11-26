#include "Assets.hpp"
#include <Engine.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

ObjectPool &AssetManager::GetObjectPool()
{
    return Engine::Get()->GetObjectPool();
}

} // namespace hyperion::v2
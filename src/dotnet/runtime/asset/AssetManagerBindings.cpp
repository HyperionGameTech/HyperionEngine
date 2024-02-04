#include <asset/Assets.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    const char *AssetManager_GetBasePath(AssetManager *manager)
    {
        return manager->GetBasePath().Data();
    }

    void AssetManager_SetBasePath(AssetManager *manager, const char *path)
    {
        manager->SetBasePath(path);
    }
}
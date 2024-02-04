#include <asset/AssetBatch.hpp>
#include <dotnet/runtime/asset/AssetMapBindings.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    AssetBatch *AssetBatch_Create(AssetManager *asset_manager)
    {
        return new AssetBatch(asset_manager);
    }

    void AssetBatch_Destroy(AssetBatch *batch)
    {
        delete batch;
    }

    void AssetBatch_LoadAsync(AssetBatch *batch)
    {
        batch->LoadAsync();
    }

    AssetMap *AssetBatch_AwaitResults(AssetBatch *batch)
    {
        return new AssetMap(batch->AwaitResults());
    }

    void AssetBatch_AddToBatch(AssetBatch *batch, const char *key, const char *path)
    {
        batch->Add(key, path);
    }
}
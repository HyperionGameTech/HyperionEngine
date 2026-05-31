/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Asset/AssetBatch.hpp>
#include <Asset/Assets.hpp>

#include <DotNET/Runtime/Asset/AssetMapBindings.hpp>

#include <Framework/EngineGlobals.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT AssetBatch* AssetBatch_Create()
    {
        return new AssetBatch(AssetManager::GetInstance());
    }

    HYP_EXPORT void AssetBatch_Destroy(AssetBatch* batch)
    {
        delete batch;
    }

    HYP_EXPORT void AssetBatch_LoadAsync(AssetBatch* batch, void (*callback)(void*))
    {
        batch->OnComplete.Bind([callback](AssetMap& assetMap)
                             {
                                 AssetMap* pNewAssetMap = new AssetMap(std::move(assetMap));

                                 // Note: Will be deleted when AssetMap_Destroy is called from C#.
                                 callback(pNewAssetMap);
                             })
            .Detach();

        batch->LoadAsync();
    }

    HYP_EXPORT AssetMap* AssetBatch_AwaitResults(AssetBatch* batch)
    {
        return new AssetMap(batch->AwaitResults());
    }

    HYP_EXPORT void AssetBatch_AddToBatch(AssetBatch* batch, const char* key, const char* path)
    {
        batch->Add(key, path);
    }

} // extern "C"

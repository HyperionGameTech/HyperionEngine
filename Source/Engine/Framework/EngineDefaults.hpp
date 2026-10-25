/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Asset/AssetPath.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/ObjectFwd.hpp>

namespace Hyperion {

class AssetObject;

HYP_STRUCT()
struct EngineTerrainLayerDefaults
{
    HYP_STRUCT_BODY(EngineTerrainLayerDefaults);

    HYP_FIELD(Property = "Albedo", Serialize, FollowAssetPath = true)
    AssetPath albedo;

    HYP_FIELD(Property = "Normal", Serialize, FollowAssetPath = true)
    AssetPath normal;
};

HYP_STRUCT()
struct EngineTerrainDefaults
{
    HYP_STRUCT_BODY(EngineTerrainDefaults);

    HYP_FIELD(Property = "GroundCover", Serialize, FollowAssetPath = true)
    AssetPath groundCover;

    HYP_FIELD(Property = "Layers", Serialize)
    Array<EngineTerrainLayerDefaults> layers;
};

HYP_STRUCT()
struct EngineDefaults
{
    HYP_STRUCT_BODY(EngineDefaults);

    HYP_FIELD(Property = "BlueNoise", Serialize, FollowAssetPath = true)
    AssetPath blueNoise;

    HYP_FIELD(Property = "SkyDome", Serialize, FollowAssetPath = true)
    AssetPath skyDome;

    HYP_FIELD(Property = "Font", Serialize, FollowAssetPath = true)
    AssetPath font;

    // only cooked for projects with terrain
    HYP_FIELD(Property = "Terrain", Serialize)
    EngineTerrainDefaults terrain;

    ENGINE_API static const EngineDefaults& Get();

    template <class T>
    static Handle<T> LoadAsset(const AssetPath& path)
    {
        return TryCast<T>(LoadAsset(path));
    }

    ENGINE_API static Handle<AssetObject> LoadAsset(const AssetPath& path);
};

} // namespace Hyperion

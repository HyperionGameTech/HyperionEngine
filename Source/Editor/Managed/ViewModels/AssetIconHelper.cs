namespace Hyperion.Editor.ViewModels
{
    /// @TODO : Don't switch on typename, use type so we don't have to add for every derived type
    /// Also.. need less retrofitting of icons, would be nice to have a different icon per unique type
    public static class AssetIconHelper
    {
        public static string FromTypeName(string? typeName) => typeName switch
        {
            "Mesh"                              => "AssetMesh",
            "Material"                          => "AssetMaterial",
            "Texture"                           => "AssetTexture",
            "DirectionalLight"                  => "DirectionalLight",
            "PointLight"                        => "PointLight",
            "SpotLight"                         => "SpotLight",
            "AreaRectLight"                     => "AreaLight",
            "Camera"                            => "Camera",
            "EnvProbe" or "ReflectionProbe"
                or "SkyProbe"                   => "EnvProbe",
            "ParticleVolume"                    => "ParticleVolume",
            "InstancedMeshProxy"
                or "InstancedMeshData"          => "AssetInstancedMesh",
            "Skeleton"                          => "AssetSkeleton",
            "Animation"                         => "AssetAnimation",
            "AnimationTrack"                    => "AssetAnimationTrack",
            "Scene"                             => "AssetScene",
            "World"                             => "AssetWorld",
            "LightmapVolume"                    => "LightmapVolume",
            "Prefab"                            => "AssetPrefab",
            "FogVolume"                         => "FogVolume",
            "Decal" or "DecalProxy"             => "Decal",
            "TextSprite"                        => "TextSprite",
            "Sprite"                            => "Sprite",
            "Entity"                            => "Entity",
            "Node"                              => "Node",
            "Shader"                            => "AssetShader",
            "ShaderBundle"                      => "AssetShaderBundle",
            "FontAtlas"                         => "AssetFontAtlas",
            "Sound" or "Audio"                  => "AssetSound",
            "PhysicsShape" or "BoxPhysicsShape"
                or "SpherePhysicsShape"
                or "PlanePhysicsShape"
                or "ConvexHullPhysicsShape"
                or "CapsulePhysicsShape"
                or "HeightFieldPhysicsShape"
                or "CompoundPhysicsShape"       => "AssetPhysicsShape",
            "Script" or "ScriptAsset"           => "AssetScript",
            "RawDataAsset"                      => "AssetRawData",
            "TerrainCellData"                   => "AssetTerrain",
            "Weapon"                            => "AssetWeapon",
            _                                   => "File",
        };

        public static string FromBucket(AssetBucket bucket)
        {
            if (bucket.Index == AssetBucket.Meshes.Index)               return "AssetMesh";
            if (bucket.Index == AssetBucket.Textures.Index)             return "AssetTexture";
            if (bucket.Index == AssetBucket.Materials.Index)            return "AssetMaterial";
            if (bucket.Index == AssetBucket.InstancedMeshData.Index)    return "AssetInstancedMesh";
            if (bucket.Index == AssetBucket.Animations.Index)           return "AssetAnimation";
            if (bucket.Index == AssetBucket.AnimationTracks.Index)      return "AssetAnimationTrack";
            if (bucket.Index == AssetBucket.Skeletons.Index)            return "AssetSkeleton";
            if (bucket.Index == AssetBucket.Worlds.Index)               return "AssetWorld";
            if (bucket.Index == AssetBucket.Scenes.Index)               return "AssetScene";
            if (bucket.Index == AssetBucket.Shaders.Index)              return "AssetShader";
            if (bucket.Index == AssetBucket.ShaderBundles.Index)        return "AssetShaderBundle";
            if (bucket.Index == AssetBucket.FontAtlases.Index)          return "AssetFontAtlas";
            if (bucket.Index == AssetBucket.PhysicsShapes.Index)        return "AssetPhysicsShape";
            if (bucket.Index == AssetBucket.Scripts.Index)              return "AssetScript";
            if (bucket.Index == AssetBucket.RawData.Index)              return "AssetRawData";
            if (bucket.Index == AssetBucket.Prefabs.Index)              return "AssetPrefab";
            if (bucket.Index == AssetBucket.Sounds.Index)               return "AssetSound";
            if (bucket.Index == AssetBucket.Terrain.Index)              return "AssetTerrain";
            if (bucket.Index == AssetBucket.Weapons.Index)              return "AssetWeapon";
            if (bucket.Index == AssetBucket.Decals.Index)               return "Decal";

            return "File";
        }
    }
}

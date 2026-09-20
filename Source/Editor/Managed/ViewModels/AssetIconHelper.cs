namespace Hyperion.Editor.ViewModels
{
    /// @TODO : Don't switch on typename, use type so we don't have to add for every derived type
    /// Also.. need less retrofitting of icons, would be nice to have a different icon per unique type
    public static class AssetIconHelper
    {
        public static string FromTypeName(string? typeName) => typeName switch
        {
            "Mesh"                              => "Package",
            "Material"                          => "Material",
            "Texture"                           => "FileMedia",
            "DirectionalLight"                  => "Lightbulb",
            "PointLight"                        => "Lightbulb",
            "SpotLight"                         => "Lightbulb",
            "AreaRectLight"                     => "Lightbulb",
            "Camera"                            => "DeviceCamera",
            "ReflectionProbe"                   => "Globe",
            "ParticleVolume"                    => "Sparkle",
            "InstancedMeshProxy"                => "Combine",
            "Skeleton"                          => "GitBranch",
            "Animation" or "AnimationTrack"     => "FileMedia",
            "Scene" or "World"                  => "Globe",
            "LightmapVolume"                    => "Package",
            "Prefab"                            => "Prefab",
            "FogVolume"                         => "Cloud",
            "Entity"                            => "CircleLarge",
            "Node"                              => "Circle",
            "Shader" or "ShaderBundle"          => "FileCode",
            "FontAtlas"                         => "CaseSensitive",
            "Sound" or "Audio"                  => "Unmute",
            "PhysicsShape"                      => "Shield",
            "Script" or "ScriptAsset"           => "FileCode",
            _                                   => "File",
        };
    }
}

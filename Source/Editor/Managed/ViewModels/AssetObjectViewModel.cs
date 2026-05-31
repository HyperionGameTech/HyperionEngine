using System;

namespace Hyperion.Editor.ViewModels
{
    public class AssetObjectViewModel : ViewModelBase
    {
        private readonly AssetDesc _assetDesc;
        private readonly AssetBucketViewModel? _bucket;
        private readonly string? _typeName;
        private readonly DateTime? _dateModified;

        public AssetDesc AssetDesc => _assetDesc;
        public AssetBucketViewModel? Bucket => _bucket;

        public string DisplayName => _assetDesc.Name.ToString();

        /// <summary>Asset class name extracted from the manifest (e.g. "MeshAsset"), or null if unavailable.</summary>
        public string? TypeName => _typeName;

        /// <summary>Last-write time of the manifest file on disk, or null if unavailable.</summary>
        public DateTime? DateModified => _dateModified;

        // @TODO Centralize this somewhere (same for scene hierarchy icons)
        public string IconKind => _typeName switch
        {
            "Mesh" or "MeshAsset"                  => "Box",
            "MaterialDefinition"                   => "Palette",
            "MaterialInstance"                     => "Paintbrush",
            "Texture" or "TextureAsset"            => "Image",
            "DirectionalLight"                     => "Sun",
            "PointLight"                           => "Lightbulb",
            "SpotLight"                            => "Spotlight",
            "AreaRectLight"                        => "RectangleHorizontal",
            "Camera"                               => "Video",
            "ReflectionProbe"                      => "Orbit",
            "ParticleVolume"                       => "Sparkles",
            "InstancedMeshProxy"                   => "SquaresUnite",
            "Skeleton"                             => "Bone",
            "Animation" or "AnimationTrack"        => "Film",
            "Scene" or "World"                     => "Globe",
            "LightmapVolume"                       => "Box",
            "FogVolume"                            => "Clouds",
            "Entity"                               => "Shapes",
            "Node"                                 => "Circle",
            "Shader" or "ShaderBundle"             => "Code2",
            "FontAtlas"                            => "Type",
            "Sound" or "Audio"                     => "Volume2",
            "PhysicsShape"                         => "Triangle",
            "Script"                               => "FileCode",
            _ when _bucket?.BucketIndex >= 0       => "File",
            _                                      => "Circle",
        };

        public AssetObjectViewModel(AssetDesc assetDesc, AssetBucketViewModel? bucket = null, string? typeName = null, DateTime? dateModified = null)
        {
            _assetDesc = assetDesc;
            _bucket = bucket;
            _typeName = typeName;
            _dateModified = dateModified;
        }
    }
}

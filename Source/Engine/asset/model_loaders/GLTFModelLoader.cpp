/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <asset/model_loaders/GLTFModelLoader.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/Loader.hpp>

#include <rendering/Shared.hpp>
#include <rendering/Material.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/Scene.hpp>
#include <scene/DetachedScene.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/MeshComponent.hpp>

#include <Core/utilities/StringUtil.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/filesystem/FsUtil.hpp>
#include <Core/filesystem/FilePath.hpp>

#include <Core/Types.hpp>
#include <Core/name/Name.hpp>

#include <Core/math/Mat4f.hpp>
#include <Core/math/Quat4f.hpp>
#include <Core/math/Vector2.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Vector4.hpp>
#include <Core/math/Transform.hpp>

#include <Core/memory/ByteBuffer.hpp>
#include <Core/containers/HashMap.hpp>
#include <Core/reflection/Handle.hpp>

#include <engine/EngineDriver.hpp>

#include <util/img/Bitmap.hpp>

#define CGLTF_IMPLEMENTATION
#include <gltf/cgltf.h>

#include <GLTFModelLoader.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);
namespace CoreApi {
extern FilePath GetExecutablePath();
}// namespace CoreApi

namespace {

using FatVertex = TVertex<VT_Simple | VT_UV1 | VT_Skeletal>;

static constexpr bool SeparateMetalnessRoughnessTextures = true;

struct GltfPrimitiveResource
{
    Handle<Mesh> mesh;
    Handle<Material> material;
    Vec3f localTranslation = Vec3f::Zero();
    uint32 gltfPrimitiveIndex = 0;
};

struct GltfMeshResource
{
    Array<GltfPrimitiveResource> primitives;
};

struct GltfLoadContext
{
    LoaderState& state;
    cgltf_data& data;
    Scene* scene;
    Array<GltfMeshResource> meshResources;
    HashMap<const cgltf_material*, Handle<Material>> materialCache;
    HashMap<const cgltf_texture*, Handle<Texture>> textureCache;
    uint32 unnamedNodeCounter = 0;
    uint32 unnamedMaterialCounter = 0;
    bool loggedMorphTargetWarning = false;
    bool loggedEmbeddedTextureWarning = false;
    bool loggedTextureWrapModeWarning = false;
};

const char* ToString(cgltf_result result)
{
    switch (result)
    {
    case cgltf_result_success:
        return "success";
    case cgltf_result_data_too_short:
        return "data_too_short";
    case cgltf_result_unknown_format:
        return "unknown_format";
    case cgltf_result_invalid_json:
        return "invalid_json";
    case cgltf_result_invalid_gltf:
        return "invalid_gltf";
    case cgltf_result_invalid_options:
        return "invalid_options";
    case cgltf_result_file_not_found:
        return "file_not_found";
    case cgltf_result_io_error:
        return "io_error";
    case cgltf_result_out_of_memory:
        return "out_of_memory";
    case cgltf_result_legacy_gltf:
        return "legacy_gltf";
    default:
        return "unknown";
    }
}

TextureFilterMode ResolveMinFilter(const cgltf_sampler* sampler)
{
    if (sampler == nullptr || sampler->min_filter == 0)
    {
        return TFM_LINEAR_MIPMAP;
    }

    switch (sampler->min_filter)
    {
    case cgltf_filter_type_nearest:
        return TFM_NEAREST;
    case cgltf_filter_type_linear:
        return TFM_LINEAR;
    case cgltf_filter_type_nearest_mipmap_nearest:
        return TFM_NEAREST_MIPMAP;
    case cgltf_filter_type_nearest_mipmap_linear:
        return TFM_NEAREST_LINEAR;
    case cgltf_filter_type_linear_mipmap_nearest:
        return TFM_LINEAR_MIPMAP;
    case cgltf_filter_type_linear_mipmap_linear:
        return TFM_LINEAR_MIPMAP;
    default:
        return TFM_LINEAR_MIPMAP;
    }
}

TextureFilterMode ResolveMagFilter(const cgltf_sampler* sampler)
{
    if (sampler == nullptr || sampler->mag_filter == 0)
    {
        return TFM_LINEAR;
    }

    switch (sampler->mag_filter)
    {
    case cgltf_filter_type_nearest:
        return TFM_NEAREST;
    case cgltf_filter_type_linear:
        return TFM_LINEAR;
    default:
        return TFM_LINEAR;
    }
}

TextureWrapMode MapWrapValue(cgltf_int wrap, GltfLoadContext& ctx)
{
    switch (wrap)
    {
    case cgltf_wrap_mode_clamp_to_edge:
        return TWM_CLAMP_TO_EDGE;
    case cgltf_wrap_mode_repeat:
        return TWM_REPEAT;
    default:
        return TWM_REPEAT;
    }
}

TextureWrapMode ResolveWrapMode(const cgltf_sampler* sampler, GltfLoadContext& ctx)
{
    if (sampler == nullptr)
    {
        return TWM_REPEAT;
    }

    const TextureWrapMode wrapS = MapWrapValue(sampler->wrap_s != 0 ? sampler->wrap_s : cgltf_wrap_mode_repeat, ctx);
    const TextureWrapMode wrapT = MapWrapValue(sampler->wrap_t != 0 ? sampler->wrap_t : cgltf_wrap_mode_repeat, ctx);

    if (wrapS == wrapT)
    {
        return wrapS;
    }

    if (wrapS == TWM_CLAMP_TO_EDGE || wrapT == TWM_CLAMP_TO_EDGE)
    {
        return TWM_CLAMP_TO_EDGE;
    }

    if (wrapS == TWM_CLAMP_TO_BORDER || wrapT == TWM_CLAMP_TO_BORDER)
    {
        return TWM_CLAMP_TO_BORDER;
    }

    return wrapS;
}

String ResolveTexturePath(GltfLoadContext& ctx, const cgltf_image& image)
{
    const FilePath baseDirectory = ctx.state.filepath.BasePath();
    const FilePath absolutePath = baseDirectory.Any()
        ? FilePath::Join(baseDirectory, image.uri ? image.uri : "")
        : FilePath(image.uri ? image.uri : "");

    const FilePath assetBasePath = ctx.state.assetManager->GetBasePath();

    if (assetBasePath.Any())
    {
        const FilePath relativeToBase = FilePath::Relative(absolutePath, assetBasePath);

        if (relativeToBase.Any())
        {
            return relativeToBase;
        }
    }

    if (absolutePath.Any())
    {
        return absolutePath;
    }

    return String(image.uri ? image.uri : "");
}

Handle<Texture> AcquireTexture(GltfLoadContext& ctx, const cgltf_texture_view& textureView, bool srgb)
{
    if (textureView.texture == nullptr)
    {
        return {};
    }

    if (const auto it = ctx.textureCache.Find(textureView.texture); it != ctx.textureCache.End())
    {
        return it->second;
    }

    Handle<Texture> textureHandle;

    const cgltf_texture& texture = *textureView.texture;
    const cgltf_image* image = texture.image;

    if (image == nullptr)
    {
        HYP_LOG(Assets, Warning, "GLTF texture missing image reference");
        ctx.textureCache.Set(textureView.texture, textureHandle);
        return textureHandle;
    }

    if (image->buffer_view != nullptr || (image->uri != nullptr && String(image->uri).StartsWith("data:")))
    {
        if (!ctx.loggedEmbeddedTextureWarning)
        {
            ctx.loggedEmbeddedTextureWarning = true;
            HYP_LOG(Assets, Warning, "GLTF embedded textures are not yet supported; skipping texture '{}'", image->name ? image->name : "<unnamed>");
        }

        ctx.textureCache.Set(textureView.texture, textureHandle);
        return textureHandle;
    }

    if (image->uri != nullptr && *image->uri)
    {
        const String texturePath = ResolveTexturePath(ctx, *image);

        auto TryLoadTexture = [&](const String& candidate) -> Handle<Texture>
        {
            if (candidate.Empty())
            {
                return {};
            }

            if (auto textureResult = ctx.state.assetManager->Load<Texture>(
                candidate,
                ctx.state.batchIdentifier,
                srgb ? AssetLoadHint::TextureLoader_LoadAsSRGB : AssetLoadHint::NoHint); textureResult.HasValue())
            {
                const Handle<Texture>& texture = textureResult->Result();
                ctx.state.assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Textures", texture);

                CheckResult(texture->Create());

                return texture;
            }

            return {};
        };

        textureHandle = TryLoadTexture(texturePath);

        if (!textureHandle)
        {
            textureHandle = TryLoadTexture(String(image->uri));
        }

        if (!textureHandle)
        {
            const String failedPath = texturePath.Any() ? texturePath : String(image->uri);
            HYP_LOG(Assets, Warning, "GLTF texture '{}' failed to load from '{}'", image->name ? image->name : failedPath.Data(), failedPath);
        }
    }
    else
    {
        HYP_LOG(Assets, Warning, "GLTF texture '{}' has no URI", image->name ? image->name : "<unnamed>");
    }

    if (!textureHandle)
    {
        ctx.textureCache.Set(textureView.texture, textureHandle);
        return textureHandle;
    }

    {
        const cgltf_sampler* sampler = texture.sampler;

        TextureDesc desc = textureHandle->GetTextureDesc();
        desc.filterModeMin = ResolveMinFilter(sampler);
        desc.filterModeMag = ResolveMagFilter(sampler);
        desc.wrapMode = ResolveWrapMode(sampler, ctx);
        textureHandle->SetTextureDesc(desc);
    }

    if (image->name && *image->name)
    {
        textureHandle->SetName(CreateNameFromDynamicString(image->name));
    }

    ctx.textureCache.Set(textureView.texture, textureHandle);

    return textureHandle;
}

const cgltf_accessor* FindAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type, cgltf_int index = 0)
{
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i)
    {
        const cgltf_attribute& attribute = primitive.attributes[i];

        if (attribute.type == type && attribute.index == index)
        {
            return attribute.data;
        }
    }

    return nullptr;
}

// https://github.com/StereoKit/StereoKit/blob/422752b4b2ec02b2933ae7959acb687b71f17f20/StereoKitC/asset_types/model_gltf.cpp#L234
// https://github.com/zhaijialong/RealEngine/blob/99fee10bf802767d4236b1bc12145b3708c65c9f/source/world/gltf_loader.cpp#L378
// https://github.com/BredaUniversityGames/DXX-Raytracer/blob/2bb9246292cd4c2e39b8553f6c3104540d493ee0/RT/Renderer/Backend/DX12/src/GLTFLoader.cpp#L26

cgltf_size UnpackAccessorFloats(const cgltf_accessor* accessor, Array<float>& outFloats)
{
    if (!accessor)
    {
        return 0;
    }

    cgltf_size size = cgltf_accessor_unpack_floats(accessor, nullptr, 0);
    outFloats.Resize(size);

    size = cgltf_accessor_unpack_floats(accessor, outFloats.Data(), size);

    if (size == 0)
    {
        outFloats.Clear();
        return 0;
    }

    return size;
}

cgltf_size UnpackAccessorIndices(const cgltf_accessor* accessor, Array<uint32>& outIndices)
{
    if (!accessor)
    {
        return 0;
    }

    outIndices.Resize(accessor->count);

    const cgltf_size unpacked = cgltf_accessor_unpack_indices(accessor, outIndices.Data(), sizeof(uint32), accessor->count);

    if (unpacked == 0)
    {
        outIndices.Clear();
        return 0;
    }

    return unpacked;
}

Name MakePrimitiveName(const cgltf_mesh& mesh, uint32 meshIndex, uint32 primitiveIndex)
{
    if (mesh.name && *mesh.name)
    {
        if (mesh.primitives_count > 1)
        {
            return NAME_FMT("{}_Primitive{}", mesh.name, primitiveIndex);
        }

        return CreateNameFromDynamicString(mesh.name);
    }

    if (mesh.primitives_count > 1)
    {
        return NAME_FMT("Mesh{}_Primitive{}", meshIndex, primitiveIndex);
    }

    return NAME_FMT("Mesh{}", meshIndex);
}

Name MakeNodeName(GltfLoadContext& ctx, const cgltf_node& node)
{
    if (node.name && *node.name)
    {
        return CreateNameFromDynamicString(node.name);
    }

    return NAME_FMT("Node{}", ctx.unnamedNodeCounter++);
}

Transform BuildTransformFromNode(const cgltf_node& node)
{
    if (node.has_matrix)
    {
        Mat4f matrix(node.matrix);
        matrix = matrix.Transpose();

        const Vec3f translation = matrix.ExtractTranslation();

        const Vec3f scale = Vec3f(
            Vec3f(matrix[0][0], matrix[1][0], matrix[2][0]).Length(),
            Vec3f(matrix[0][1], matrix[1][1], matrix[2][1]).Length(),
            Vec3f(matrix[0][2], matrix[1][2], matrix[2][2]).Length());

        Quat4f rotation = matrix.ExtractRotation().Inverse();
        rotation.Normalize();

        return Transform(translation, scale, rotation);
    }

    Vec3f translation(0.0f);
    Vec3f scale(1.0f);
    Quat4f rotation = Quat4f::Identity();

    if (node.has_translation)
    {
        translation = Vec3f(
            float(node.translation[0]),
            float(node.translation[1]),
            float(node.translation[2]));
    }

    if (node.has_scale)
    {
        scale = Vec3f(
            float(node.scale[0]),
            float(node.scale[1]),
            float(node.scale[2]));
    }

    if (node.has_rotation)
    {
        rotation = Quat4f(
            float(node.rotation[0]),
            float(node.rotation[1]),
            float(node.rotation[2]),
            float(node.rotation[3])).Inverse();
        rotation.Normalize();
    }

    return Transform(translation, scale, rotation);
}

struct SplitMetalnessRoughnessResult
{
    Handle<Texture> metalness;
    Handle<Texture> roughness;
};

SplitMetalnessRoughnessResult SplitMetalnessRoughnessTexture(
    GltfLoadContext& ctx,
    const Handle<Texture>& combinedTexture,
    const Name& baseName)
{
    if (!combinedTexture)
    {
        return {};
    }

    const TextureDesc& srcDesc = combinedTexture->GetTextureDesc();

    auto resGuard = combinedTexture->GetReadScope();
    const ConstByteView imageData = combinedTexture->GetImageData();

    if (!imageData)
    {
        HYP_LOG(Assets, Warning, "GLTF metallic-roughness texture '{}' has no CPU-side image data available for channel splitting; skipping", baseName);
        return {};
    }

    const uint32 numComponents = TextureUtils::NumComponents(srcDesc.format);
    const uint32 bytesPerComponent = TextureUtils::BytesPerComponent(srcDesc.format);
    const uint32 width = srcDesc.extent.x;
    const uint32 height = srcDesc.extent.y;
    const uint32 numPixels = width * height;

    if (numComponents < 3 || bytesPerComponent != 1)
    {
        HYP_LOG(Assets, Warning, "GLTF metallic-roughness texture '{}' has unsupported format for channel splitting; skipping separate textures", baseName);
        return {};
    }

    const ubyte* src = static_cast<const ubyte*>(imageData.Data());
    const uint32 stride = numComponents;

    Bitmap<TextureFormat::R8> roughnessBitmap(width, height);
    Bitmap<TextureFormat::R8> metalnessBitmap(width, height);

    for (uint32 row = 0; row < height; ++row)
    {
        for (uint32 col = 0; col < width; ++col)
        {
            const ubyte* pixel = src + (row * width + col) * stride;
            roughnessBitmap.GetPixelReference(col, row).SetR(float(pixel[1]) / 255.0f);
            metalnessBitmap.GetPixelReference(col, row).SetR(float(pixel[2]) / 255.0f);
        }
    }

    TextureDesc channelDesc;
    channelDesc.type = TextureType::Texture2D;
    channelDesc.format = TextureFormat::R8;
    channelDesc.extent = Vec3u { width, height, 1 };
    channelDesc.filterModeMin = srcDesc.filterModeMin;
    channelDesc.filterModeMag = srcDesc.filterModeMag;
    channelDesc.wrapMode = srcDesc.wrapMode;

    ByteBuffer roughnessData(roughnessBitmap.ToByteView());
    ByteBuffer metalnessData(metalnessBitmap.ToByteView());

    Texture::GenerateMipmaps(channelDesc, roughnessData);
    // metalness shares the same dimensions so mip layout is identical
    TextureDesc metalnessDesc = channelDesc;
    Texture::GenerateMipmaps(metalnessDesc, metalnessData);

    Handle<Texture> roughnessTexture = MakeHandle<Texture>(channelDesc, roughnessData.ToByteView());
    roughnessTexture->SetName(NAME_FMT("{}_Roughness", baseName));
    ctx.state.assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Textures", roughnessTexture);
    CheckResult(roughnessTexture->Create());

    Handle<Texture> metalnessTexture = MakeHandle<Texture>(metalnessDesc, metalnessData.ToByteView());
    metalnessTexture->SetName(NAME_FMT("{}_Metalness", baseName));
    ctx.state.assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Textures", metalnessTexture);
    CheckResult(metalnessTexture->Create());

    return { metalnessTexture, roughnessTexture };
}

Handle<Material> AcquireMaterial(GltfLoadContext& ctx, const cgltf_material* material, const Handle<Mesh>& mesh)
{
    MaterialAttributes materialAttributes {};
    materialAttributes.shaderName = NAME("GeometryPass");
    materialAttributes.shaderProperties = {};
    materialAttributes.bucket = RenderBucket::Opaque;

    if (material == nullptr)
    {
        Handle<Material> fallback = MaterialCache::GetInstance()->GetOrCreate(
            NAME("BasicGLTFMaterial"),
            materialAttributes,
            MaterialParameters {});

        InitObject(fallback);

        return fallback;
    }

    if (const auto it = ctx.materialCache.Find(material); it != ctx.materialCache.End())
    {
        return it->second;
    }

    const Name materialName = (material->name && *material->name)
        ? CreateNameFromDynamicString(material->name)
        : NAME_FMT("Material{}", ctx.unnamedMaterialCounter++);

    Vec4f baseColor(1.0f, 1.0f, 1.0f, 1.0f);
    float metallic = 0.0f;
    float roughness = 1.0f;
    float transmission = 0.0f;

    MaterialTextures textures;

    if (material->has_pbr_metallic_roughness)
    {
        const auto& pbr = material->pbr_metallic_roughness;

        baseColor = Vec4f(
            float(pbr.base_color_factor[0]),
            float(pbr.base_color_factor[1]),
            float(pbr.base_color_factor[2]),
            float(pbr.base_color_factor[3]));

        metallic = float(pbr.metallic_factor);
        roughness = float(pbr.roughness_factor);

        if (Handle<Texture> baseColorTexture = AcquireTexture(ctx, pbr.base_color_texture, /* srgb */ true); baseColorTexture.IsValid())
        {
            textures[MaterialTextureKey::Diffuse] = baseColorTexture;
        }

        if (Handle<Texture> metallicRoughnessTexture = AcquireTexture(ctx, pbr.metallic_roughness_texture, false); metallicRoughnessTexture.IsValid())
        {
            if constexpr (SeparateMetalnessRoughnessTextures)
            {
                auto [metalnessTexture, roughnessTexture] = SplitMetalnessRoughnessTexture(ctx, metallicRoughnessTexture, materialName);

                if (metalnessTexture)
                {
                    textures[MaterialTextureKey::Metalness] = metalnessTexture;
                }

                if (roughnessTexture)
                {
                    textures[MaterialTextureKey::Roughness] = roughnessTexture;
                }
            }
            else
            {
                textures[MaterialTextureKey::Metalness] = metallicRoughnessTexture;
                textures[MaterialTextureKey::Roughness] = metallicRoughnessTexture;
            }
        }
    }

    if (material->has_transmission)
    {
        transmission = float(material->transmission.transmission_factor);
    }

    MaterialParameters parameters;
    parameters.albedo = baseColor;
    parameters.metalness = metallic;
    parameters.roughness = roughness;

    if (transmission > 0.0f)
    {
        parameters.transmission = transmission;
    }

    if (material->alpha_mode == cgltf_alpha_mode_mask)
    {
        parameters.alphaThreshold = float(material->alpha_cutoff);
    }

    switch (material->alpha_mode)
    {
    case cgltf_alpha_mode_blend:
        materialAttributes.bucket = RenderBucket::Translucent;
        materialAttributes.blendFunction = BlendFunction::AlphaBlending();
        break;
    case cgltf_alpha_mode_mask:
        materialAttributes.flags |= MAF_ALPHA_DISCARD;
        break;
    default:
        break;
    }

    if (material->double_sided)
    {
        materialAttributes.cullFaces = FCM_NONE;
    }

    if (Handle<Texture> normalTexture = AcquireTexture(ctx, material->normal_texture, false); normalTexture.IsValid())
    {
        textures[MaterialTextureKey::Normals] = normalTexture;
    }

    if (Handle<Texture> occlusionTexture = AcquireTexture(ctx, material->occlusion_texture, false); occlusionTexture.IsValid())
    {
        textures[MaterialTextureKey::AmbientOcclusion] = occlusionTexture;
    }

    const Vec3f emissiveFactor(
        float(material->emissive_factor[0]),
        float(material->emissive_factor[1]),
        float(material->emissive_factor[2]));

    if (emissiveFactor != Vec3f::Zero())
    {
        parameters.emissiveColor = Vec4f(emissiveFactor, 1.0f);
    }

    Handle<Material> materialHandle = MaterialCache::GetInstance()->CreateMaterial(materialName, materialAttributes, parameters, textures);
    InitObject(materialHandle);

    ctx.materialCache.Set(material, materialHandle);

    return materialHandle;
}

struct PrimitiveBuildOutput
{
    Handle<Mesh> mesh;
    Vec3f localTranslation = Vec3f::Zero();
};

bool BuildPrimitive(GltfLoadContext& ctx,
    const cgltf_mesh& gltfMesh,
    const cgltf_primitive& primitive,
    uint32 meshIndex,
    uint32 primitiveIndex,
    PrimitiveBuildOutput& out)
{
    Topology topology;
    switch (primitive.type)
    {
    case cgltf_primitive_type_triangles:
        topology = TOP_TRIANGLES;
        break;
    case cgltf_primitive_type_triangle_strip:
        topology = TOP_TRIANGLE_STRIP;
        break;
    case cgltf_primitive_type_triangle_fan:
        topology = TOP_TRIANGLE_FAN;
        break;
    default:
        HYP_LOG(Assets, Warning, "GLTF primitive skipped due to unsupported topology {} on mesh '{}'",
            int(primitive.type),
            gltfMesh.name ? gltfMesh.name : "<unnamed>");
        return false;
    }

    const cgltf_accessor* positionsAccessor = FindAttribute(primitive, cgltf_attribute_type_position);
    if (!positionsAccessor)
    {
        HYP_LOG(Assets, Warning, "GLTF primitive skipped: missing POSITION attribute on mesh '{}'",
            gltfMesh.name ? gltfMesh.name : "<unnamed>");
        return false;
    }

    const cgltf_size vertexCount = positionsAccessor->count;
    if (vertexCount == 0)
    {
        return false;
    }

    const cgltf_accessor* normalsAccessor = FindAttribute(primitive, cgltf_attribute_type_normal);
    const cgltf_accessor* tangentAccessor = FindAttribute(primitive, cgltf_attribute_type_tangent);
    const cgltf_accessor* texcoord0Accessor = FindAttribute(primitive, cgltf_attribute_type_texcoord, 0);
    const cgltf_accessor* texcoord1Accessor = FindAttribute(primitive, cgltf_attribute_type_texcoord, 1);
    const cgltf_accessor* jointsAccessor = FindAttribute(primitive, cgltf_attribute_type_joints, 0);
    const cgltf_accessor* weightsAccessor = FindAttribute(primitive, cgltf_attribute_type_weights, 0);

    if (primitive.targets_count != 0 && !ctx.loggedMorphTargetWarning)
    {
        ctx.loggedMorphTargetWarning = true;
        HYP_LOG(Assets, Warning, "GLTF morph targets are not currently supported and will be ignored");
    }

    Array<float> positionsData;
    if (UnpackAccessorFloats(positionsAccessor, positionsData) == 0)
    {
        HYP_LOG(Assets, Warning, "Failed to unpack POSITION data from buffer view for mesh '{}'",
            gltfMesh.name ? gltfMesh.name : "<unnamed>");
        return false;
    }

    Array<float> normalsData;
    const bool hasNormals = normalsAccessor != nullptr && UnpackAccessorFloats(normalsAccessor, normalsData) != 0;

    Array<float> tangentsData;
    const bool hasTangents = tangentAccessor != nullptr && UnpackAccessorFloats(tangentAccessor, tangentsData) != 0;

    Array<float> texcoord0Data;
    const bool hasTexcoord0 = texcoord0Accessor != nullptr && UnpackAccessorFloats(texcoord0Accessor, texcoord0Data) != 0;

    Array<float> texcoord1Data;
    const bool hasTexcoord1 = texcoord1Accessor != nullptr && UnpackAccessorFloats(texcoord1Accessor, texcoord1Data) != 0;

    Array<float> jointsFloatData;
    Array<float> weightsData;

    const bool hasSkinning = jointsAccessor != nullptr && weightsAccessor != nullptr
        && UnpackAccessorFloats(jointsAccessor, jointsFloatData) != 0
        && UnpackAccessorFloats(weightsAccessor, weightsData) != 0;

    Array<FatVertex> vertices;
    vertices.Resize(vertexCount);

    bool hasBounds = false;
    Vec3f meshAabbMin = Vec3f::Zero();
    Vec3f meshAabbMax = Vec3f::Zero();

    for (cgltf_size vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        FatVertex vertex;

        {
            const cgltf_size base = vertexIndex * 3;
            const Vec3f position(positionsData[base], positionsData[base + 1], positionsData[base + 2]);
            vertex.SetPosition(position);

            if (!hasBounds)
            {
                meshAabbMin = position;
                meshAabbMax = position;
                hasBounds = true;
            }
            else
            {
                if (position.x < meshAabbMin.x)
                {
                    meshAabbMin.x = position.x;
                }

                if (position.y < meshAabbMin.y)
                {
                    meshAabbMin.y = position.y;
                }

                if (position.z < meshAabbMin.z)
                {
                    meshAabbMin.z = position.z;
                }

                if (position.x > meshAabbMax.x)
                {
                    meshAabbMax.x = position.x;
                }

                if (position.y > meshAabbMax.y)
                {
                    meshAabbMax.y = position.y;
                }

                if (position.z > meshAabbMax.z)
                {
                    meshAabbMax.z = position.z;
                }
            }
        }

        if (hasNormals)
        {
            const cgltf_size base = vertexIndex * 3;
            vertex.SetNormal(Vec3f(normalsData[base], normalsData[base + 1], normalsData[base + 2]));
        }

        if (hasTexcoord0)
        {
            const cgltf_size base = vertexIndex * 2;
            vertex.SetUV0(Vec2f(texcoord0Data[base], 1.0f - texcoord0Data[base + 1]));
        }

        if (hasTexcoord1)
        {
            const cgltf_size base = vertexIndex * 2;
            vertex.SetUV1(Vec2f(texcoord1Data[base], 1.0f - texcoord1Data[base + 1]));
        }

        if (hasSkinning)
        {
            const cgltf_size base = vertexIndex * 4;

            for (uint32 i = 0; i < 4; ++i)
            {
                vertex.SetBoneIndex(i, uint8(jointsFloatData[base + i]));
                vertex.SetBoneWeight(i, weightsData[base + i]);
            }
        }

        vertices[vertexIndex] = vertex;
    }

    if (hasBounds)
    {
        const Vec3f meshAabbCenter = (meshAabbMin + meshAabbMax) * 0.5f;

        // offset vertices so that the mesh is centered around the origin
        for (FatVertex& vertex : vertices)
        {
            vertex.SetPosition(vertex.GetPosition() - meshAabbCenter);
        }

        out.localTranslation = meshAabbCenter;
    }

    Array<uint32> indices;

    if (primitive.indices != nullptr)
    {
        if (UnpackAccessorIndices(primitive.indices, indices) == 0)
        {
            HYP_LOG(Assets, Warning, "Failed to unpack index data from buffer view for mesh '{}'",
                gltfMesh.name ? gltfMesh.name : "<unnamed>");
            return false;
        }
    }
    else
    {
        indices.Resize(vertexCount);

        for (cgltf_size i = 0; i < vertexCount; ++i)
        {
            indices[i] = uint32(i);
        }
    }

    MeshDesc meshDesc;
    meshDesc.meshAttributes.inputLayout = VertexInputLayoutDesc { VT_Simple | VT_UV1 | VT_Skeletal };
    meshDesc.meshAttributes.topology = topology;
    meshDesc.meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    meshDesc.numVertices = uint32(vertices.Size());
    meshDesc.numIndices = uint32(indices.Size());

    const Name assetName = MakePrimitiveName(gltfMesh, meshIndex, primitiveIndex);

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(assetName);
    
    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.vertexCount = vertices.Size();
    vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;

    mesh->SetMeshData(meshDesc, vertexArrayView, indices.ToByteView());

    if (!hasNormals && meshDesc.meshAttributes.topology == TOP_TRIANGLES)
    {
        mesh->CalculateNormals();
    }

    mesh->SetOriginalFilepath(FilePath::Relative(ctx.state.filepath, ctx.state.assetManager->GetBasePath()));

    ctx.state.assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Meshes", mesh);

    InitObject(mesh);

    out.mesh = mesh;

    return true;
}

Name DetermineRootName(const LoaderState& state, const cgltf_data& data)
{
    if (data.scene && data.scene->name && *data.scene->name)
    {
        return CreateNameFromDynamicString(data.scene->name);
    }

    for (cgltf_size i = 0; i < data.scenes_count; ++i)
    {
        if (data.scenes[i].name && *data.scenes[i].name)
        {
            return CreateNameFromDynamicString(data.scenes[i].name);
        }
    }

    String basename = state.filepath.Basename();

    if (!basename.Empty())
    {
        return CreateNameFromDynamicString(StringUtil::StripExtension(basename));
    }

    return NAME("GLTFModel");
}

Handle<Node> BuildNodeRecursive(GltfLoadContext& ctx, const cgltf_node& node)
{
    const Name nodeName = MakeNodeName(ctx, node);

    Handle<Node> nodeHandle = MakeHandle<Node>(nodeName);
    nodeHandle->SetLocalTransform(BuildTransformFromNode(node));

    if (node.skin != nullptr)
    {
        HYP_LOG_ONCE(Assets, Warning, "GLTF skinning is not yet supported; meshes will be loaded without skin influences");
    }

    if (node.mesh != nullptr)
    {
        const uint32 meshIndex = uint32(node.mesh - ctx.data.meshes);

        if (meshIndex < ctx.meshResources.Size())
        {
            const GltfMeshResource& meshResource = ctx.meshResources[meshIndex];

            for (const GltfPrimitiveResource& primitive : meshResource.primitives)
            {
                if (!primitive.mesh.IsValid())
                {
                    continue;
                }

                Handle<Entity> entity = ctx.scene->GetEntityManager()->AddEntity();

                const Name entityName = NAME_FMT("{}_Primitive{}", nodeName, primitive.gltfPrimitiveIndex);
                entity->SetName(entityName);
                entity->SetLocalBounds(primitive.mesh->GetAABB());
                entity->SetLocalTranslation(primitive.localTranslation);

                ctx.scene->GetEntityManager()->AddComponent<MeshComponent>(entity, MeshComponent { primitive.mesh, primitive.material });

                nodeHandle->AddChild(entity);
            }
        }
        else
        {
            HYP_LOG(Assets, Warning, "GLTF node '{}' references out-of-range mesh index {}", nodeName, meshIndex);
        }
    }

    for (cgltf_size childIndex = 0; childIndex < node.children_count; ++childIndex)
    {
        const cgltf_node* childNode = node.children[childIndex];

        if (childNode == nullptr)
        {
            continue;
        }

        Handle<Node> childHandle = BuildNodeRecursive(ctx, *childNode);
        nodeHandle->AddChild(childHandle);
    }

    return nodeHandle;
}

LoadedAsset BuildModel(LoaderState& state, cgltf_data& data)
{
    Scene* scene = GetDetachedSceneForCurrentThread();

    GltfLoadContext ctx {
        .state = state,
        .data = data,
        .scene = scene
    };

    ctx.meshResources.Resize(data.meshes_count);

    for (cgltf_size meshIndex = 0; meshIndex < data.meshes_count; ++meshIndex)
    {
        const cgltf_mesh& gltfMesh = data.meshes[meshIndex];
        GltfMeshResource& meshResource = ctx.meshResources[meshIndex];
        meshResource.primitives.Reserve(gltfMesh.primitives_count);

        for (cgltf_size primitiveIndex = 0; primitiveIndex < gltfMesh.primitives_count; ++primitiveIndex)
        {
            PrimitiveBuildOutput output;

            if (!BuildPrimitive(ctx, gltfMesh, gltfMesh.primitives[primitiveIndex], uint32(meshIndex), uint32(primitiveIndex), output))
            {
                continue;
            }

            Handle<Material> material = AcquireMaterial(ctx, gltfMesh.primitives[primitiveIndex].material, output.mesh);
            InitObject(material);

            meshResource.primitives.PushBack(GltfPrimitiveResource {
                .mesh = output.mesh,
                .material = material,
                .localTranslation = output.localTranslation,
                .gltfPrimitiveIndex = uint32(primitiveIndex)
            });
        }

        if (meshResource.primitives.Empty())
        {
            HYP_LOG(Assets, Warning, "GLTF mesh '{}' produced no renderable primitives",
                gltfMesh.name ? gltfMesh.name : "<unnamed>");
        }
    }

    Array<const cgltf_node*> rootNodes;

    if (data.scene && data.scene->nodes_count != 0)
    {
        rootNodes.Reserve(data.scene->nodes_count);

        for (cgltf_size nodeIndex = 0; nodeIndex < data.scene->nodes_count; ++nodeIndex)
        {
            if (data.scene->nodes[nodeIndex] != nullptr)
            {
                rootNodes.PushBack(data.scene->nodes[nodeIndex]);
            }
        }
    }
    else
    {
        rootNodes.Reserve(data.nodes_count);

        for (cgltf_size nodeIndex = 0; nodeIndex < data.nodes_count; ++nodeIndex)
        {
            if (data.nodes[nodeIndex].parent == nullptr)
            {
                rootNodes.PushBack(&data.nodes[nodeIndex]);
            }
        }
    }

    const Name rootName = DetermineRootName(state, data);
    
    Handle<Node> root = MakeHandle<Node>(rootName);
    root->SetIsDynamic(false);

    for (const cgltf_node* rootNode : rootNodes)
    {
        if (!rootNode)
        {
            continue;
        }

        Handle<Node> child = BuildNodeRecursive(ctx, *rootNode);
        root->AddChild(child);
    }

    return LoadedAsset { root };
}

} // namespace

GLTFModelLoader::GLTFModelLoader() = default;

AssetLoadResult GLTFModelLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);

    cgltf_options options {};
    cgltf_data* data = nullptr;

    HYP_DEFER({
        if (data)
        {
            cgltf_free(data);
            data = nullptr;
        }
    });

    const FilePath absPath = state.filepath;

    const cgltf_result parseResult = cgltf_parse_file(&options, absPath.Data(), &data);

    if (parseResult != cgltf_result_success || data == nullptr)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to parse glTF file '{}': {}", absPath, ToString(parseResult));
    }

    const cgltf_result bufferResult = cgltf_load_buffers(&options, data, absPath.Data());

    if (bufferResult != cgltf_result_success)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "Failed to load glTF buffers '{}': {}", absPath, ToString(bufferResult));
    }

    const cgltf_result validationResult = cgltf_validate(data);

    if (validationResult != cgltf_result_success)
    {
        HYP_LOG(Assets, Warning, "GLTF validation warning for '{}': {}", absPath, ToString(validationResult));

        return HYP_MAKE_ERROR(AssetLoadError, "GLTF validation failed for '{}': {}", absPath, ToString(validationResult));
    }

    return BuildModel(state, *data);
}

} // namespace Hyperion

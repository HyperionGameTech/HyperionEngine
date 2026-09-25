/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <AssetPch.hpp>

#include <Asset/ModelLoaders/GLTFModelLoader.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/Loader.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>

#include <Scene/World.hpp>
#include <Scene/Node.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/DetachedScene.hpp>

#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/AnimationComponent.hpp>

#include <Core/Utilities/StringUtil.hpp>
#include <Core/Utilities/Optional.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/Constants.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Containers/Set.hpp>
#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FsUtil.hpp>
#include <Core/FileSystem/FilePath.hpp>

#include <Core/Types.hpp>
#include <Core/Name/Name.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Framework/EngineDriver.hpp>

#include <Util/Img/Bitmap.hpp>
#include <Util/Img/ImageUtil.hpp>

#include <Core/Utilities/Span.hpp>

#include <stb_image.h>

#include <cstring>
#include <algorithm>

#define CGLTF_IMPLEMENTATION
#include <gltf/cgltf.h>

#include <GLTFModelLoader.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
} // namespace CoreApi

namespace {

using FatVertex = TVertex<VT_Simple | VT_UV1 | VT_Skeletal>;

static constexpr bool SeparateMetalnessRoughnessTextures = true;

struct GltfPrimitiveResource
{
    Handle<Mesh> mesh;
    Handle<Material> material;
    Vec3f localTranslation = Vec3f::Zero();
    uint32 gltfPrimitiveIndex = 0;
    bool skinned = false;
};

struct GltfMeshResource
{
    Array<GltfPrimitiveResource> primitives;
};

struct GltfSkinResource
{
    Handle<Skeleton> skeleton;
    Array<uint32> jointToBoneIndex;
    Array<const cgltf_node*> orderedJointNodes;
    Map<const cgltf_node*, Name> boneNames;
    Set<const cgltf_node*> jointNodes;
};

struct GltfLoadContext
{
    LoaderState& state;
    cgltf_data& data;
    Scene* scene;
    Array<GltfMeshResource> meshResources;
    Map<const cgltf_skin*, GltfSkinResource> skinResources;
    Map<const cgltf_material*, Handle<Material>> materialCache;
    Map<const cgltf_texture*, Handle<Texture>> textureCache;
    uint32 unnamedNodeCounter = 0;
    uint32 unnamedMaterialCounter = 0;
    bool loggedMorphTargetWarning = false;
    bool loggedTextureWrapModeWarning = false;
    bool loggedJointIndexOutOfRangeWarning = false;
    bool loggedSkinnedPrimitiveWithoutInfluencesWarning = false;
};

static bool ShouldRegisterAssets(const LoaderState& state)
{
    return !bool(state.hint & AssetLoadHint::Transient);
}

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
        return TextureFilterMode::LinearMipmap;
    }

    switch (sampler->min_filter)
    {
    case cgltf_filter_type_nearest:
        return TextureFilterMode::Nearest;
    case cgltf_filter_type_linear:
        return TextureFilterMode::Linear;
    case cgltf_filter_type_nearest_mipmap_nearest:
        return TextureFilterMode::NearestMipmap;
    case cgltf_filter_type_nearest_mipmap_linear:
        return TextureFilterMode::NearestLinear;
    case cgltf_filter_type_linear_mipmap_nearest:
        return TextureFilterMode::LinearMipmap;
    case cgltf_filter_type_linear_mipmap_linear:
        return TextureFilterMode::LinearMipmap;
    default:
        return TextureFilterMode::LinearMipmap;
    }
}

TextureFilterMode ResolveMagFilter(const cgltf_sampler* sampler)
{
    if (sampler == nullptr || sampler->mag_filter == 0)
    {
        return TextureFilterMode::Linear;
    }

    switch (sampler->mag_filter)
    {
    case cgltf_filter_type_nearest:
        return TextureFilterMode::Nearest;
    case cgltf_filter_type_linear:
        return TextureFilterMode::Linear;
    default:
        return TextureFilterMode::Linear;
    }
}

TextureWrapMode MapWrapValue(cgltf_int wrap, GltfLoadContext& ctx)
{
    switch (wrap)
    {
    case cgltf_wrap_mode_clamp_to_edge:
        return TextureWrapMode::ClampToEdge;
    case cgltf_wrap_mode_repeat:
        return TextureWrapMode::Repeat;
    default:
        return TextureWrapMode::Repeat;
    }
}

TextureWrapMode ResolveWrapMode(const cgltf_sampler* sampler, GltfLoadContext& ctx)
{
    if (sampler == nullptr)
    {
        return TextureWrapMode::Repeat;
    }

    const TextureWrapMode wrapS = MapWrapValue(sampler->wrap_s != 0 ? sampler->wrap_s : cgltf_wrap_mode_repeat, ctx);
    const TextureWrapMode wrapT = MapWrapValue(sampler->wrap_t != 0 ? sampler->wrap_t : cgltf_wrap_mode_repeat, ctx);

    if (wrapS == wrapT)
    {
        return wrapS;
    }

    if (wrapS == TextureWrapMode::ClampToEdge || wrapT == TextureWrapMode::ClampToEdge)
    {
        return TextureWrapMode::ClampToEdge;
    }

    if (wrapS == TextureWrapMode::ClampToBorder || wrapT == TextureWrapMode::ClampToBorder)
    {
        return TextureWrapMode::ClampToBorder;
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

        if (relativeToBase.Any() && !relativeToBase.StartsWith(".."))
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

Handle<Texture> LoadTextureFromEncodedBytes(const cgltf_image& image, Span<const ubyte> encodedBytes, bool srgb)
{
    const char* debugName = (image.name && *image.name) ? image.name : "<unnamed>";

    if (encodedBytes.Size() == 0)
    {
        HYP_LOG(Assets, Warning, "Embedded GLTF texture '{}' has no image data", debugName);
        return {};
    }

    if (encodedBytes.Size() > size_t(MathUtil::MaxSafeValue<int>()))
    {
        HYP_LOG(Assets, Warning, "Embedded GLTF texture '{}' is too large to decode ({} bytes)", debugName, encodedBytes.Size());
        return {};
    }

    if (image.mime_type && *image.mime_type)
    {
        const String mimeType(image.mime_type);

        if (mimeType.Contains("ktx") || mimeType.Contains("basis"))
        {
            HYP_LOG(Assets, Warning, "Embedded GLTF texture '{}' uses unsupported compressed format '{}'", debugName, image.mime_type);
            return {};
        }
    }

    int width = 0;
    int height = 0;
    int numComponents = 0;

    ubyte* imageBytes = stbi_load_from_memory(
        encodedBytes.Data(),
        int(encodedBytes.Size()),
        &width,
        &height,
        &numComponents,
        0);

    if (imageBytes == nullptr || width <= 0 || height <= 0)
    {
        HYP_LOG(Assets, Warning, "Failed to decode embedded GLTF texture '{}': {}", debugName, stbi_failure_reason());
        return {};
    }

    HYP_DEFER({
        stbi_image_free(imageBytes);
    });

    TextureFormat format;

    switch (numComponents)
    {
    case STBI_rgb_alpha:
        format = TextureFormat::RGBA8;
        break;
    case STBI_rgb:
        format = TextureFormat::RGB8;
        break;
    case STBI_grey_alpha:
        format = TextureFormat::RG8;
        break;
    case STBI_grey:
        format = TextureFormat::R8;
        break;
    default:
        HYP_LOG(Assets, Warning, "Embedded GLTF texture '{}' has unsupported component count {}", debugName, numComponents);
        return {};
    }

    TextureDesc textureDesc {
        TextureType::Texture2D,
        format,
        Vec3u { uint32(width), uint32(height), 1 },
        TextureFilterMode::LinearMipmap,
        TextureFilterMode::Linear,
        TextureWrapMode::Repeat
    };

    const size_t imageBytesCount = size_t(width)
        * size_t(height)
        * size_t(numComponents);

    ByteBuffer baseMipData = ByteBuffer(imageBytesCount, imageBytes);

    if (numComponents == 3)
    {
        // convert to bytes per pixel = 4
        const uint32 faceOffsetStep = textureDesc.GetByteSize() / textureDesc.NumArrayLayers();

        textureDesc.format = TextureUtils::FormatChangeNumComponents(format, 4);

        const uint32 newFaceOffsetStep = textureDesc.GetByteSize() / textureDesc.NumArrayLayers();

        ByteBuffer newByteBuffer(textureDesc.GetByteSize());

        for (uint32 i = 0; i < textureDesc.NumArrayLayers(); i++)
        {
            ImageUtil::ConvertBPP(
                textureDesc.extent.x, textureDesc.extent.y, textureDesc.extent.z,
                numComponents, 4,
                &baseMipData.Data()[i * faceOffsetStep],
                &newByteBuffer.Data()[i * newFaceOffsetStep]);
        }

        baseMipData = std::move(newByteBuffer);
    }

    if (srgb)
    {
        textureDesc.format = TextureUtils::ChangeFormatSRGB(textureDesc.format, /* useSRGB */ true);
    }

    Texture::GenerateMipmaps(textureDesc, baseMipData);

    return MakeHandle<Texture>(textureDesc, baseMipData.ToByteView());
}

Handle<Texture> LoadEmbeddedTexture(GltfLoadContext& ctx, const cgltf_image& image, bool srgb)
{
    if (image.buffer_view != nullptr)
    {
        const uint8_t* bufferViewData = cgltf_buffer_view_data(image.buffer_view);

        if (bufferViewData == nullptr)
        {
            HYP_LOG(Assets, Warning, "Embedded GLTF texture '{}' has inaccessible buffer view", image.name ? image.name : "<unnamed>");
            return {};
        }

        return LoadTextureFromEncodedBytes(image, Span<const ubyte>(bufferViewData, size_t(image.buffer_view->size)), srgb);
    }

    if (image.uri != nullptr && String(image.uri).StartsWith("data:"))
    {
        const char* comma = strchr(image.uri, ',');

        if (comma == nullptr)
        {
            HYP_LOG(Assets, Warning, "Embedded GLTF texture '{}' has malformed data URI", image.name ? image.name : "<unnamed>");
            return {};
        }

        const char* base64 = comma + 1;
        const cgltf_size base64Length = strlen(base64);

        cgltf_size decodedSize = (base64Length / 4) * 3;

        if (base64Length >= 1 && base64[base64Length - 1] == '=')
        {
            decodedSize--;
        }

        if (base64Length >= 2 && base64[base64Length - 2] == '=')
        {
            decodedSize--;
        }

        void* decodedData = nullptr;

        cgltf_options decodeOptions {};
        const cgltf_result decodeResult = cgltf_load_buffer_base64(&decodeOptions, decodedSize, base64, &decodedData);

        if (decodeResult != cgltf_result_success || decodedData == nullptr)
        {
            HYP_LOG(Assets, Warning, "Failed to decode base64 data URI for GLTF texture '{}': {}", image.name ? image.name : "<unnamed>", ToString(decodeResult));
            return {};
        }

        HYP_DEFER({
            void (*freeFunc)(void*, void*) = ctx.data.memory.free_func ? ctx.data.memory.free_func : &cgltf_default_free;
            freeFunc(ctx.data.memory.user_data, decodedData);
        });

        return LoadTextureFromEncodedBytes(image, Span<const ubyte>(static_cast<const ubyte*>(decodedData), size_t(decodedSize)), srgb);
    }

    return {};
}

Handle<Texture> AcquireTexture(LoaderState& state, GltfLoadContext& ctx, const cgltf_texture_view& textureView, bool srgb)
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
        textureHandle = LoadEmbeddedTexture(ctx, *image, srgb);

        if (!textureHandle)
        {
            ctx.textureCache.Set(textureView.texture, textureHandle);
            return textureHandle;
        }
    }
    else if (image->uri != nullptr && *image->uri)
    {
        const String texturePath = ResolveTexturePath(ctx, *image);

        auto tryLoadTexture = [&](const String& candidate) -> Handle<Texture>
        {
            if (candidate.Empty())
            {
                return {};
            }

            if (auto textureResult = ctx.state.assetManager->Load<Texture>(
                    candidate,
                    ctx.state.batchIdentifier,
                    (state.hint & ~AssetLoadHint::TextureSRGB) | (srgb ? AssetLoadHint::TextureSRGB : AssetLoadHint::NoHint));

                textureResult.HasValue())
            {
                const Handle<Texture>& texture = textureResult->Result();

                if (ShouldRegisterAssets(ctx.state))
                {
                    GetCurrentAssetRegistry()->PutAssetUnique(texture);
                }

                return texture;
            }

            return {};
        };

        textureHandle = tryLoadTexture(texturePath);

        if (!textureHandle)
        {
            textureHandle = tryLoadTexture(String(image->uri));
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

    if (ShouldRegisterAssets(ctx.state))
    {
        GetCurrentAssetRegistry()->PutAssetUnique(textureHandle);
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

// cgltf_validate's size check can overflow on a huge count, and accessors without a buffer view aren't bounded at all
static bool IsAccessorCountSane(const cgltf_accessor* accessor)
{
    static constexpr cgltf_size MaxElementsWithoutBufferView = cgltf_size(1) << 24;

    if (accessor->count == 0)
    {
        return true;
    }

    const cgltf_buffer_view* bufferView = accessor->buffer_view;

    if (bufferView == nullptr)
    {
        return accessor->count <= MaxElementsWithoutBufferView;
    }

    const cgltf_size elementSize = cgltf_calc_size(accessor->type, accessor->component_type);
    const cgltf_size stride = accessor->stride != 0 ? accessor->stride : elementSize;

    if (elementSize == 0 || accessor->offset > bufferView->size || bufferView->size - accessor->offset < elementSize)
    {
        return false;
    }

    return accessor->count - 1 <= (bufferView->size - accessor->offset - elementSize) / stride;
}

cgltf_size UnpackAccessorFloats(const cgltf_accessor* accessor, Array<float>& outFloats)
{
    if (!accessor || !IsAccessorCountSane(accessor))
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
    if (!accessor || !IsAccessorCountSane(accessor))
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

// glTF matrices are column-major and right-handed; returns the engine-layout matrix mirrored across Z
Mat4f ConvertGltfMatrix(const float* columnMajorValues)
{
    Mat4f matrix = Mat4f(columnMajorValues).Transpose();

    // Conjugate with D = diag(1, 1, -1, 1) to mirror across Z: M' = D * M * D
    matrix[0][2] *= -1.0f;
    matrix[1][2] *= -1.0f;
    matrix[2][0] *= -1.0f;
    matrix[2][1] *= -1.0f;
    matrix[2][3] *= -1.0f;
    matrix[3][2] *= -1.0f;

    return matrix;
}

Transform DecomposeMatrix(Mat4f matrix)
{
    const Vec3f translation = matrix.ExtractTranslation();

    Vec3f scale = Vec3f(
        Vec3f(matrix[0][0], matrix[1][0], matrix[2][0]).Length(),
        Vec3f(matrix[0][1], matrix[1][1], matrix[2][1]).Length(),
        Vec3f(matrix[0][2], matrix[1][2], matrix[2][2]).Length());

    // A negative determinant means the matrix contains a reflection;
    // fold the mirror into the X axis so a proper rotation can be extracted
    if (matrix.Determinant() < 0.0f)
    {
        scale.x = -scale.x;

        matrix[0][0] *= -1.0f;
        matrix[1][0] *= -1.0f;
        matrix[2][0] *= -1.0f;
    }

    // Mat4f::Rotation builds the transposed rotation matrix, so Transform stores
    // rotations inverted relative to the standard quaternion convention
    Quat4f rotation = matrix.ExtractRotation().Inverse();
    rotation.Normalize();

    return Transform(translation, scale, rotation);
}

Transform BuildTransformFromNode(const cgltf_node& node)
{
    if (node.has_matrix)
    {
        return DecomposeMatrix(ConvertGltfMatrix(node.matrix));
    }

    Vec3f translation(0.0f);
    Vec3f scale(1.0f);
    Quat4f rotation = Quat4f::Identity();

    if (node.has_translation)
    {
        translation = Vec3f(
            float(node.translation[0]),
            float(node.translation[1]),
            -float(node.translation[2]));
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
        // mirror - we use LHS; inverted to match the transposed convention of Mat4f::Rotation
        rotation = Quat4f(
                       -float(node.rotation[0]),
                       -float(node.rotation[1]),
                       float(node.rotation[2]),
                       float(node.rotation[3]))
                       .Inverse();
        rotation.Normalize();
    }

    return Transform(translation, scale, rotation);
}

// Matches the world matrix the node gets in the built hierarchy (the model root is identity)
Mat4f BuildWorldMatrix(const cgltf_node* node)
{
    Mat4f matrix = Mat4f::Identity();

    for (; node != nullptr; node = node->parent)
    {
        matrix = BuildTransformFromNode(*node).GetMatrix() * matrix;
    }

    return matrix;
}

GltfSkinResource BuildSkinResource(GltfLoadContext& ctx, const cgltf_skin& skin)
{
    GltfSkinResource resource;

    for (cgltf_size jointIndex = 0; jointIndex < skin.joints_count; ++jointIndex)
    {
        if (skin.joints[jointIndex] != nullptr)
        {
            resource.jointNodes.Insert(skin.joints[jointIndex]);
        }
    }

    if (resource.jointNodes.Empty())
    {
        HYP_LOG(Assets, Warning, "GLTF skin '{}' has no joints; no skeleton will be created",
                skin.name ? skin.name : "<unnamed>");

        return resource;
    }

    if (skin.joints_count > MaxBonesPerSkeleton)
    {
        HYP_LOG(Assets, Warning, "GLTF skin '{}' has {} joints, but only {} bones are supported per skeleton; skinning of excess bones will be incorrect",
                skin.name ? skin.name : "<unnamed>",
                uint32(skin.joints_count),
                MaxBonesPerSkeleton);
    }

    const Name skinName = (skin.name && *skin.name)
        ? CreateNameFromDynamicString(skin.name)
        : NAME_FMT("Skin{}", ctx.skinResources.Size());

    // Skinned vertices are transformed by the mesh entity's world matrix after skinning, so the skeleton is
    // built in the space of the mesh node using this skin (glTF itself ignores that node's transform)
    const cgltf_node* skinnedMeshNode = nullptr;
    Mat4f meshWorldMatrix = Mat4f::Identity();

    for (cgltf_size nodeIndex = 0; nodeIndex < ctx.data.nodes_count; ++nodeIndex)
    {
        const cgltf_node& node = ctx.data.nodes[nodeIndex];

        if (node.skin != &skin || node.mesh == nullptr)
        {
            continue;
        }

        const Mat4f nodeWorldMatrix = BuildWorldMatrix(&node);

        if (skinnedMeshNode == nullptr)
        {
            skinnedMeshNode = &node;
            meshWorldMatrix = nodeWorldMatrix;
        }
        else if (nodeWorldMatrix != meshWorldMatrix)
        {
            HYP_LOG(Assets, Warning, "GLTF skin '{}' is used by mesh nodes with different transforms; only '{}' will be skinned correctly",
                    skinName, skinnedMeshNode->name ? skinnedMeshNode->name : "<unnamed>");

            break;
        }
    }

    const Mat4f inverseMeshWorldMatrix = meshWorldMatrix.Inverse();

    // The bind pose comes from the inverse bind matrices, which often differ from the joints' node transforms
    Array<float> inverseBindData;
    const bool hasInverseBindMatrices = skin.inverse_bind_matrices != nullptr
        && UnpackAccessorFloats(skin.inverse_bind_matrices, inverseBindData) >= skin.joints_count * 16;

    if (skin.inverse_bind_matrices != nullptr && !hasInverseBindMatrices)
    {
        HYP_LOG(Assets, Warning, "GLTF skin '{}' has too few inverse bind matrices; treating them as identity", skinName);
    }

    Map<const cgltf_node*, Mat4f> bindMatrices; // joint bind pose in the mesh node's space

    for (cgltf_size jointIndex = 0; jointIndex < skin.joints_count; ++jointIndex)
    {
        const cgltf_node* jointNode = skin.joints[jointIndex];

        if (jointNode == nullptr || bindMatrices.Find(jointNode) != bindMatrices.End())
        {
            continue;
        }

        bindMatrices.Set(jointNode, hasInverseBindMatrices
                ? ConvertGltfMatrix(inverseBindData.Data() + jointIndex * 16).Inverse()
                : Mat4f::Identity());
    }

    Map<const cgltf_node*, const cgltf_node*> jointParents;
    Set<const cgltf_node*> topLevelParentNodes;

    for (const auto& bindIt : bindMatrices)
    {
        const cgltf_node* jointParent = nullptr;

        for (const cgltf_node* parentNode = bindIt.first->parent; parentNode != nullptr; parentNode = parentNode->parent)
        {
            if (resource.jointNodes.Contains(parentNode))
            {
                jointParent = parentNode;

                break;
            }
        }

        jointParents.Set(bindIt.first, jointParent);

        if (jointParent == nullptr)
        {
            topLevelParentNodes.Insert(bindIt.first->parent);
        }
    }

    Handle<Bone> rootBone = MakeHandle<Bone>(NAME_FMT("{}_Root", skinName));

    Map<const cgltf_node*, Handle<Bone>> topLevelParentBones;
    Map<const cgltf_node*, Mat4f> topLevelParentMatrices;

    for (const cgltf_node* parentNode : topLevelParentNodes)
    {
        const Mat4f parentMatrix = inverseMeshWorldMatrix * BuildWorldMatrix(parentNode);

        Handle<Bone> parentBone = rootBone;

        if (topLevelParentNodes.Size() > 1)
        {
            parentBone = MakeHandle<Bone>(NAME_FMT("{}_Parent{}", skinName, topLevelParentBones.Size()));
            rootBone->AddChild(parentBone);
        }

        parentBone->SetBindingTransform(DecomposeMatrix(parentMatrix));

        topLevelParentBones.Set(parentNode, parentBone);
        topLevelParentMatrices.Set(parentNode, parentMatrix);
    }

    Array<Name> usedBoneNames;
    Map<const cgltf_node*, Handle<Bone>> bonesByNode;

    uint32 unnamedJointCounter = 0;

    for (cgltf_size jointIndex = 0; jointIndex < skin.joints_count; ++jointIndex)
    {
        const cgltf_node* jointNode = skin.joints[jointIndex];

        if (jointNode == nullptr || bonesByNode.Find(jointNode) != bonesByNode.End())
        {
            continue;
        }

        const cgltf_node* jointParent = jointParents.At(jointNode);

        const Mat4f& parentBindMatrix = jointParent != nullptr
            ? bindMatrices.At(jointParent)
            : topLevelParentMatrices.At(jointNode->parent);

        const Transform bindingTransform = DecomposeMatrix(parentBindMatrix.Inverse() * bindMatrices.At(jointNode));

        Name boneName = (jointNode->name && *jointNode->name)
            ? CreateNameFromDynamicString(jointNode->name)
            : NAME_FMT("{}_Joint{}", skinName, unnamedJointCounter++);

        if (usedBoneNames.Contains(boneName))
        {
            uint32 suffix = 0;
            Name candidateName;

            do
            {
                candidateName = NAME_FMT("{}_{}", boneName, suffix++);
            }
            while (usedBoneNames.Contains(candidateName));

            boneName = candidateName;
        }

        usedBoneNames.PushBack(boneName);

        Handle<Bone> bone = MakeHandle<Bone>(boneName);
        bone->SetBindingTransform(bindingTransform);

        bonesByNode.Set(jointNode, bone);
        resource.orderedJointNodes.PushBack(jointNode);
        resource.boneNames.Set(jointNode, boneName);
    }

    for (const auto& boneIt : bonesByNode)
    {
        const cgltf_node* jointParent = jointParents.At(boneIt.first);

        const Handle<Bone>& parentBone = jointParent != nullptr
            ? bonesByNode.At(jointParent)
            : topLevelParentBones.At(boneIt.first->parent);

        parentBone->AddChild(boneIt.second);
    }

    Handle<Skeleton> skeleton = MakeHandle<Skeleton>();
    skeleton->SetName(skinName);
    skeleton->SetRootBone(rootBone);

    // Mirror the traversal done in Skeleton::UpdateRenderProxy, so that the joint -> bone index
    // remapping matches the bone matrix layout used by the skinning shader
    Map<const Bone*, uint32> boneIndices;
    boneIndices.Set(rootBone.Get(), 0);

    uint32 traversalIndex = 1;

    for (Node* descendant : rootBone->GetDescendants())
    {
        if (Bone* bone = DynamicCast<Bone>(descendant))
        {
            boneIndices.Set(bone, traversalIndex);
        }

        ++traversalIndex;
    }

    for (cgltf_size jointIndex = 0; jointIndex < skin.joints_count; ++jointIndex)
    {
        uint32 boneIndex = 0;

        if (skin.joints[jointIndex] != nullptr)
        {
            const auto boneIt = boneIndices.Find(bonesByNode.At(skin.joints[jointIndex]).Get());

            if (boneIt != boneIndices.End())
            {
                boneIndex = boneIt->second;
            }
        }

        resource.jointToBoneIndex.PushBack(boneIndex);
    }

    if (Bone* rootBonePtr = skeleton->GetRootBone())
    {
        rootBonePtr->SetToBindingPose();
        rootBonePtr->StoreBindingPose();
        rootBonePtr->ClearPose();
    }

    if (ShouldRegisterAssets(ctx.state))
    {
        GetCurrentAssetRegistry()->PutAssetUnique(skeleton);
    }

    InitObject(skeleton);

    resource.skeleton = skeleton;

    return resource;
}

struct GltfChannelSamples
{
    Array<float> times;
    Array<float> values;
    uint32 numComponents = 0;
    cgltf_size stride = 0;
    cgltf_size valueOffset = 0;
    bool step = false;

    bool Load(const cgltf_animation_sampler& sampler, uint32 components)
    {
        const bool cubic = sampler.interpolation == cgltf_interpolation_type_cubic_spline;

        numComponents = components;
        step = sampler.interpolation == cgltf_interpolation_type_step;

        // Cubic spline samplers store an in-tangent, the value and an out-tangent per keyframe
        stride = numComponents * (cubic ? 3 : 1);
        valueOffset = cubic ? numComponents : 0;

        if (UnpackAccessorFloats(sampler.input, times) == 0
            || UnpackAccessorFloats(sampler.output, values) < times.Size() * stride)
        {
            times.Clear();
            values.Clear();

            return false;
        }

        return true;
    }

    bool IsValid() const
    {
        return times.Any();
    }

    // Cubic spline tangents are ignored; spline values are interpolated linearly
    void Sample(float time, float* out) const
    {
        const float* timesBegin = times.Data();
        const cgltf_size nextIndex = cgltf_size(std::upper_bound(timesBegin, timesBegin + times.Size(), time) - timesBegin);

        const auto valueAt = [this](cgltf_size keyIndex) -> const float*
        {
            return values.Data() + keyIndex * stride + valueOffset;
        };

        if (step || nextIndex == 0 || nextIndex >= times.Size())
        {
            const float* value = valueAt(nextIndex == 0 ? 0 : nextIndex - 1);

            for (uint32 component = 0; component < numComponents; ++component)
            {
                out[component] = value[component];
            }

            return;
        }

        const cgltf_size previousIndex = nextIndex - 1;
        const float alpha = (time - times[previousIndex]) / (times[nextIndex] - times[previousIndex]);

        const float* from = valueAt(previousIndex);
        const float* to = valueAt(nextIndex);

        if (numComponents == 4)
        {
            Quat4f rotation(from[0], from[1], from[2], from[3]);
            rotation.Slerp(Quat4f(to[0], to[1], to[2], to[3]), alpha);
            rotation.Normalize();

            out[0] = rotation.x;
            out[1] = rotation.y;
            out[2] = rotation.z;
            out[3] = rotation.w;

            return;
        }

        for (uint32 component = 0; component < numComponents; ++component)
        {
            out[component] = MathUtil::Lerp(from[component], to[component], alpha);
        }
    }
};

struct GltfJointChannels
{
    GltfChannelSamples translation;
    GltfChannelSamples rotation;
    GltfChannelSamples scale;
};

Handle<Animation> BuildAnimationForSkin(const LoaderState& state, const cgltf_animation& animation, uint32 animationIndex, const GltfSkinResource& skinResource)
{
    const Name animationName = (animation.name && *animation.name)
        ? CreateNameFromDynamicString(animation.name)
        : NAME_FMT("Animation{}", animationIndex);

    Handle<Animation> result = MakeHandle<Animation>(animationName);

    Map<const cgltf_node*, GltfJointChannels> channelsByJoint;

    for (cgltf_size channelIndex = 0; channelIndex < animation.channels_count; ++channelIndex)
    {
        const cgltf_animation_channel& channel = animation.channels[channelIndex];

        if (channel.target_node == nullptr || channel.sampler == nullptr)
        {
            continue;
        }

        if (!skinResource.jointNodes.Contains(channel.target_node))
        {
            // Only bones can be animated; channels targeting other nodes are ignored
            continue;
        }

        GltfJointChannels& jointChannels = channelsByJoint[channel.target_node];

        GltfChannelSamples* samples = nullptr;
        uint32 numComponents = 3;

        switch (channel.target_path)
        {
        case cgltf_animation_path_type_translation:
            samples = &jointChannels.translation;
            break;
        case cgltf_animation_path_type_rotation:
            samples = &jointChannels.rotation;
            numComponents = 4;
            break;
        case cgltf_animation_path_type_scale:
            samples = &jointChannels.scale;
            break;
        default:
            // Morph target weights are not supported; a warning is emitted when loading mesh data
            continue;
        }

        if (!samples->Load(*channel.sampler, numComponents))
        {
            HYP_LOG(Assets, Warning, "GLTF animation '{}' channel {} has missing or insufficient keyframe data; skipping channel",
                    animationName, uint32(channelIndex));
        }
    }

    for (const cgltf_node* jointNode : skinResource.orderedJointNodes)
    {
        const Transform restLocalTransform = BuildTransformFromNode(*jointNode);
        const GltfJointChannels* jointChannels = nullptr;

        if (const auto channelsIt = channelsByJoint.Find(jointNode); channelsIt != channelsByJoint.End())
        {
            jointChannels = &channelsIt->second;
        }

        Array<float> keyTimes;

        if (jointChannels != nullptr)
        {
            for (const GltfChannelSamples* samples : { &jointChannels->translation, &jointChannels->rotation, &jointChannels->scale })
            {
                keyTimes.Concat(samples->times);
            }
        }

        std::sort(keyTimes.Begin(), keyTimes.End());

        Array<float> uniqueKeyTimes;
        uniqueKeyTimes.Reserve(keyTimes.Size());

        for (const float keyTime : keyTimes)
        {
            if (uniqueKeyTimes.Empty() || uniqueKeyTimes.Back() != keyTime)
            {
                uniqueKeyTimes.PushBack(keyTime);
            }
        }

        if (uniqueKeyTimes.Empty())
        {
            uniqueKeyTimes.PushBack(0.0f);
        }

        Array<Keyframe> keyframes;
        keyframes.Reserve(uniqueKeyTimes.Size());

        for (const float keyTime : uniqueKeyTimes)
        {
            Transform transform = restLocalTransform;
            float value[4];

            if (jointChannels != nullptr && jointChannels->translation.IsValid())
            {
                jointChannels->translation.Sample(keyTime, value);
                transform.translation = Vec3f(value[0], value[1], -value[2]);
            }

            if (jointChannels != nullptr && jointChannels->scale.IsValid())
            {
                jointChannels->scale.Sample(keyTime, value);
                transform.scale = Vec3f(value[0], value[1], value[2]);
            }

            if (jointChannels != nullptr && jointChannels->rotation.IsValid())
            {
                jointChannels->rotation.Sample(keyTime, value);

                // mirror - we use LHS; inverted to match the transposed convention of Mat4f::Rotation
                transform.rotation = Quat4f(-value[0], -value[1], value[2], value[3]).Inverse();
                transform.rotation.Normalize();
            }

            keyframes.PushBack(Keyframe(keyTime, transform));
        }

        const Name boneName = skinResource.boneNames.At(jointNode);

        Handle<AnimationTrack> track = MakeHandle<AnimationTrack>(
            NAME_FMT("{}_{}", animationName, boneName),
            boneName);

        track->SetKeyframes(keyframes);

        if (ShouldRegisterAssets(state))
        {
            GetCurrentAssetRegistry()->PutAssetUnique(track);
        }

        result->AddTrack(track);
    }

    return result;
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

    Handle<Texture> metalnessTexture = MakeHandle<Texture>(metalnessDesc, metalnessData.ToByteView());
    metalnessTexture->SetName(NAME_FMT("{}_Metalness", baseName));

    if (ShouldRegisterAssets(ctx.state))
    {
        GetCurrentAssetRegistry()->PutAssetUnique(roughnessTexture);
        GetCurrentAssetRegistry()->PutAssetUnique(metalnessTexture);
    }

    return { metalnessTexture, roughnessTexture };
}

static constexpr const char* BlendModeExtraKey = "hyperionBlendMode";

// glTF only knows straight alpha blending, so other blend modes (or blending a MASK material) are asked for in the material's extras
Optional<BlendFunction> ReadBlendModeOverride(const cgltf_material* gltfMaterial, Name materialName)
{
    if (gltfMaterial->extras.data == nullptr)
    {
        return {};
    }

    const JSON::ParseResult parseResult = JSON::Parse(UTF8StringView(gltfMaterial->extras.data));

    if (!parseResult.ok)
    {
        return {};
    }

    const auto blendModeValue = parseResult.value[BlendModeExtraKey];

    if (!blendModeValue.IsString())
    {
        return {};
    }

    const String blendMode = blendModeValue.AsString().ToLower();

    if (blendMode == "alpha")
    {
        return BlendFunction::AlphaBlending();
    }

    if (blendMode == "premultiplied")
    {
        return BlendFunction::PremultipliedAlpha();
    }

    if (blendMode == "additive")
    {
        return BlendFunction::Additive();
    }

    HYP_LOG(Assets, Warning, "GLTF material {} has unknown {} '{}' (expected alpha, premultiplied or additive)", materialName, BlendModeExtraKey, blendMode);

    return {};
}

Handle<Material> AcquireMaterial(LoaderState& state, GltfLoadContext& ctx, const cgltf_material* gltfMaterial, const Handle<Mesh>& mesh)
{
    MaterialAttributes materialAttributes {};
    materialAttributes.shaderName = NAME("GeometryPass");
    materialAttributes.shaderProperties = {};
    materialAttributes.bucket = RenderBucket::Opaque;

    if (!gltfMaterial)
    {
        Handle<Material> fallbackMaterial = MakeHandle<Material>(
            NAME("BasicGLTFMaterial"),
            materialAttributes,
            MaterialParameters {},
            MaterialTextures {});

        InitObject(fallbackMaterial);

        if (ShouldRegisterAssets(state))
        {
            GetCurrentAssetRegistry()->PutAsset(fallbackMaterial);
        }

        return fallbackMaterial;
    }

    if (const auto it = ctx.materialCache.Find(gltfMaterial); it != ctx.materialCache.End())
    {
        return it->second;
    }

    const Name materialName = (gltfMaterial->name && *gltfMaterial->name)
        ? CreateNameFromDynamicString(gltfMaterial->name)
        : NAME_FMT("Material{}", ctx.unnamedMaterialCounter++);

    Vec4f baseColor(1.0f, 1.0f, 1.0f, 1.0f);
    float metallic = 0.0f;
    float roughness = 1.0f;
    float transmission = 0.0f;

    MaterialTextures textures;

    if (gltfMaterial->has_pbr_metallic_roughness)
    {
        const auto& pbr = gltfMaterial->pbr_metallic_roughness;

        baseColor = Vec4f(
            float(pbr.base_color_factor[0]),
            float(pbr.base_color_factor[1]),
            float(pbr.base_color_factor[2]),
            float(pbr.base_color_factor[3]));

        metallic = float(pbr.metallic_factor);
        roughness = float(pbr.roughness_factor);

        if (Handle<Texture> baseColorTexture = AcquireTexture(state, ctx, pbr.base_color_texture, /* srgb */ true); baseColorTexture.IsValid())
        {
            textures[MaterialTextureKey::Diffuse] = baseColorTexture;
        }

        if (Handle<Texture> metallicRoughnessTexture = AcquireTexture(state, ctx, pbr.metallic_roughness_texture, false); metallicRoughnessTexture.IsValid())
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

    if (gltfMaterial->has_transmission)
    {
        transmission = float(gltfMaterial->transmission.transmission_factor);
    }

    MaterialParameters parameters;
    parameters.albedo = baseColor;
    parameters.metalness = metallic;
    parameters.roughness = roughness;

    if (transmission > 0.0f)
    {
        parameters.transmission = transmission;
    }

    if (gltfMaterial->alpha_mode == cgltf_alpha_mode_mask)
    {
        parameters.alphaThreshold = float(gltfMaterial->alpha_cutoff);
    }

    Optional<BlendFunction> translucentBlendFunction;

    switch (gltfMaterial->alpha_mode)
    {
    case cgltf_alpha_mode_blend:
        translucentBlendFunction = BlendFunction::AlphaBlending();
        break;
    case cgltf_alpha_mode_mask:
        materialAttributes.flags |= MAF_ALPHA_DISCARD;
        break;
    default:
        break;
    }

    if (Optional<BlendFunction> blendModeOverride = ReadBlendModeOverride(gltfMaterial, materialName); blendModeOverride.HasValue())
    {
        translucentBlendFunction = blendModeOverride;
    }

    if (translucentBlendFunction.HasValue())
    {
        materialAttributes.bucket = RenderBucket::Translucent;
        materialAttributes.blendFunction = *translucentBlendFunction;

        // blended surfaces aren't sorted, so writing depth would hide whatever translucent draws after them
        materialAttributes.flags &= ~MAF_DEPTH_WRITE;
    }

    if (gltfMaterial->double_sided)
    {
        materialAttributes.cullFaces = FaceCullMode::None;
    }

    if (Handle<Texture> normalTexture = AcquireTexture(state, ctx, gltfMaterial->normal_texture, false); normalTexture.IsValid())
    {
        textures[MaterialTextureKey::Normals] = normalTexture;
    }

    if (Handle<Texture> occlusionTexture = AcquireTexture(state, ctx, gltfMaterial->occlusion_texture, false); occlusionTexture.IsValid())
    {
        textures[MaterialTextureKey::AmbientOcclusion] = occlusionTexture;
    }

    const Vec3f emissiveFactor(
        float(gltfMaterial->emissive_factor[0]),
        float(gltfMaterial->emissive_factor[1]),
        float(gltfMaterial->emissive_factor[2]));

    bool useEmissiveTextureAsDiffuse = false;

    if (gltfMaterial->emissive_texture.texture != nullptr)
    {
        if (Handle<Texture> emissiveTexture = AcquireTexture(state, ctx, gltfMaterial->emissive_texture, /* srgb */ true); emissiveTexture.IsValid())
        {
            if (!textures.Has(MaterialTextureKey::Diffuse))
            {
                // some exporters bake the base color into the emissive map
                textures[MaterialTextureKey::Diffuse] = emissiveTexture;
                useEmissiveTextureAsDiffuse = true;
            }
        }
    }

    // there's no emissive map slot, so a factor that the map was meant to mask would light up the whole surface
    const bool hasUnusableEmissiveTexture = gltfMaterial->emissive_texture.texture != nullptr && !useEmissiveTextureAsDiffuse;

    if (emissiveFactor != Vec3f::Zero() && !useEmissiveTextureAsDiffuse && !hasUnusableEmissiveTexture)
    {
        const float emissiveStrength = gltfMaterial->has_emissive_strength
            ? float(gltfMaterial->emissive_strength.emissive_strength)
            : 1.0f;

        parameters.emissiveIntensity = emissiveFactor.Length();
        parameters.emissiveColor = Color(Vec4f(emissiveFactor / parameters.emissiveIntensity, 1.0f));
        parameters.emissiveIntensity *= emissiveStrength;
    }

    Handle<Material> material = MakeHandle<Material>(
        materialName,
        materialAttributes,
        parameters,
        textures);

    if (ShouldRegisterAssets(state))
    {
        GetCurrentAssetRegistry()->PutAsset(material);
    }

    InitObject(material);

    ctx.materialCache.Set(gltfMaterial, material);

    return material;
}

using TreeVertex = TVertex<VT_Simple | VT_Tree>;
using FoliageVertex = TVertex<VT_Simple | VT_Tree | VT_Foliage>;

static constexpr const char* ArborTreeWindExtension = "ARBOR_tree_wind";
static constexpr uint32 ArborBranchFloats = 8;

struct ArborTreeWind
{
    Array<float> branches;
    Array<float> leafOrigins;

    // trunk, then each sway order
    float flexibility[NumTreeSwayOrders + 1] {};
    float frequency = 0.0f;
    float flutter = 0.0f;
    float height = 0.0f;
};

bool ReadArborTreeWind(const GltfLoadContext& ctx, const cgltf_primitive& primitive, ArborTreeWind& outWind)
{
    const cgltf_extension* extension = nullptr;

    for (cgltf_size extensionIndex = 0; extensionIndex < primitive.extensions_count; ++extensionIndex)
    {
        const cgltf_extension& candidate = primitive.extensions[extensionIndex];

        if (candidate.name != nullptr && candidate.data != nullptr && std::strcmp(candidate.name, ArborTreeWindExtension) == 0)
        {
            extension = &candidate;
            break;
        }
    }

    if (extension == nullptr)
    {
        return false;
    }

    const JSON::ParseResult parseResult = JSON::Parse(UTF8StringView(extension->data));

    if (!parseResult.ok)
    {
        HYP_LOG(Assets, Warning, "GLTF {} extension could not be parsed: {}", ArborTreeWindExtension, parseResult.message);
        return false;
    }

    const JSON::Value& json = parseResult.value;

    const auto readFloat = [](const auto& value) -> float
    {
        return value.IsNumber() ? float(value.ToNumber()) : 0.0f;
    };

    const auto findAccessor = [&ctx](const auto& indexValue) -> const cgltf_accessor*
    {
        if (!indexValue.IsNumber())
        {
            return nullptr;
        }

        const cgltf_size accessorIndex = cgltf_size(indexValue.ToNumber());

        return accessorIndex < ctx.data.accessors_count ? &ctx.data.accessors[accessorIndex] : nullptr;
    };

    if (UnpackAccessorFloats(findAccessor(json["branches"]), outWind.branches) < ArborBranchFloats)
    {
        HYP_LOG(Assets, Warning, "GLTF {} extension has no branch table; the mesh will not sway", ArborTreeWindExtension);
        return false;
    }

    UnpackAccessorFloats(findAccessor(json["leafOrigins"]), outWind.leafOrigins);

    if (const auto flexibility = json["flexibility"]; flexibility.IsArray())
    {
        const JSON::JArray& values = flexibility.AsArray();

        for (uint32 orderIndex = 0; orderIndex < NumTreeSwayOrders + 1 && orderIndex < values.Size(); ++orderIndex)
        {
            outWind.flexibility[orderIndex] = values[orderIndex].ToFloat();
        }
    }

    outWind.frequency = readFloat(json["frequency"]);
    outWind.flutter = readFloat(json["flutter"]);
    outWind.height = readFloat(json["height"]);

    return true;
}

// Deflection along a cantilever under an even load: 0 at the fixed end, 1 at the free one
float Cantilever(float x)
{
    x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);

    return x * x * (6.0f - 4.0f * x + x * x) / 3.0f;
}

// Tree meshes keep the glTF origin at the tree's foot, so shaders take the trunk's sway from mesh space height
template <uint8 TMask>
void BuildTreeVertices(
    const ArborTreeWind& wind,
    const Array<FatVertex>& vertices,
    const Array<float>& branchCoordinates,
    Array<TVertex<TMask>>& outVertices)
{
    const uint32 numBranches = uint32(wind.branches.Size() / ArborBranchFloats);

    // Same conversion the positions went through: right to left handed
    const auto toMeshSpace = [](const float* gltfPoint) -> Vec3f
    {
        return Vec3f(gltfPoint[0], gltfPoint[1], -gltfPoint[2]);
    };

    outVertices.Resize(vertices.Size());

    for (size_t vertexIndex = 0; vertexIndex < vertices.Size(); ++vertexIndex)
    {
        const FatVertex& source = vertices[vertexIndex];
        TVertex<TMask>& vertex = outVertices[vertexIndex];

        vertex.SetPosition(source.GetPosition());
        vertex.SetNormal(source.GetNormal());
        vertex.SetUV0(source.GetUV0());

        uint32 branchIndex = uint32(branchCoordinates[vertexIndex * 2]);
        float along = branchCoordinates[vertexIndex * 2 + 1];

        // Walk from the vertex's own branch out to its limb, one order at a time
        for (uint32 step = 0; step < NumTreeSwayOrders && branchIndex != 0 && branchIndex < numBranches; ++step)
        {
            const float* branch = &wind.branches[branchIndex * ArborBranchFloats];
            const uint32 order = uint32(branch[6]);

            if (order >= NumTreeSwayOrders)
            {
                break;
            }

            const float weight = branch[3] * Cantilever(along) * wind.flexibility[order + 1];
            vertex.SetSwayOrder(order, Vec4f(toMeshSpace(branch), weight));

            branchIndex = uint32(branch[4]);
            along = branch[5];
        }

        if constexpr ((TMask & VT_Foliage) != 0)
        {
            vertex.SetFoliage(Vec4f(toMeshSpace(&wind.leafOrigins[vertexIndex * 3]), 1.0f));
        }
    }
}

struct PrimitiveBuildOutput
{
    Handle<Mesh> mesh;
    Vec3f localTranslation = Vec3f::Zero();
    bool skinned = false;
    bool tree = false;
    bool foliage = false;
    float windFrequency = 0.0f;
    float windTrunkFlexibility = 0.0f;
    float windTreeHeight = 0.0f;
    float windFlutter = 0.0f;
};

bool BuildPrimitive(GltfLoadContext& ctx,
                    const cgltf_mesh& gltfMesh,
                    const cgltf_primitive& primitive,
                    uint32 meshIndex,
                    uint32 primitiveIndex,
                    const Array<uint32>* jointRemap,
                    PrimitiveBuildOutput& out)
{
    Topology topology;
    switch (primitive.type)
    {
    case cgltf_primitive_type_triangles:
        topology = Topology::Triangles;
        break;
    case cgltf_primitive_type_triangle_strip:
        topology = Topology::TriangleStrip;
        break;
    case cgltf_primitive_type_triangle_fan:
        topology = Topology::TriangleFan;
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

    if (positionsAccessor->type != cgltf_type_vec3)
    {
        HYP_LOG(Assets, Warning, "GLTF primitive skipped: POSITION attribute is not VEC3 on mesh '{}'",
                gltfMesh.name ? gltfMesh.name : "<unnamed>");
        return false;
    }

    Array<float> positionsData;
    if (UnpackAccessorFloats(positionsAccessor, positionsData) < vertexCount * 3)
    {
        HYP_LOG(Assets, Warning, "Failed to unpack POSITION data from buffer view for mesh '{}'",
                gltfMesh.name ? gltfMesh.name : "<unnamed>");
        return false;
    }

    Array<float> normalsData;
    const bool hasNormals = normalsAccessor != nullptr
        && normalsAccessor->type == cgltf_type_vec3
        && UnpackAccessorFloats(normalsAccessor, normalsData) >= vertexCount * 3;

    Array<float> tangentsData;
    const bool hasTangents = tangentAccessor != nullptr && UnpackAccessorFloats(tangentAccessor, tangentsData) != 0;

    Array<float> texcoord0Data;
    const bool hasTexcoord0 = texcoord0Accessor != nullptr
        && texcoord0Accessor->type == cgltf_type_vec2
        && UnpackAccessorFloats(texcoord0Accessor, texcoord0Data) >= vertexCount * 2;

    Array<float> texcoord1Data;
    const bool hasTexcoord1 = texcoord1Accessor != nullptr && UnpackAccessorFloats(texcoord1Accessor, texcoord1Data) != 0;

    Array<float> jointsFloatData;
    Array<float> weightsData;

    const bool hasSkinning = jointsAccessor != nullptr && weightsAccessor != nullptr
        && jointsAccessor->type == cgltf_type_vec4
        && weightsAccessor->type == cgltf_type_vec4
        && UnpackAccessorFloats(jointsAccessor, jointsFloatData) >= vertexCount * 4
        && UnpackAccessorFloats(weightsAccessor, weightsData) >= vertexCount * 4;

    Array<FatVertex> vertices;
    vertices.Resize(vertexCount);

    BoundingBox bounds;

    for (cgltf_size vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
    {
        FatVertex vertex;

        {
            const cgltf_size base = vertexIndex * 3;

            Vec3f position = {
                positionsData[base],
                positionsData[base + 1],
                positionsData[base + 2]
            };

            // Invert Z to convert Right-Handed to Left-Handed
            position.z *= -1.0f;

            vertex.SetPosition(position);

            bounds = bounds.Union(position);
        }

        if (hasNormals)
        {
            const cgltf_size base = vertexIndex * 3;

            Vec3f normal = {
                normalsData[base],
                normalsData[base + 1],
                normalsData[base + 2]
            };

            // Same deal as above, we need to flip the normal Z to account for our conversion to use left handed coordinates.
            normal.z *= -1.0f;

            vertex.SetNormal(normal);
        }

        if (hasTexcoord0)
        {
            const cgltf_size base = vertexIndex * 2;
            vertex.SetUV0(Vec2f(texcoord0Data[base], 1.0f - texcoord0Data[base + 1]));
        }

        // Disabled UV1 in loader for now.

        // if (hasTexcoord1)
        //{
        //     const cgltf_size base = vertexIndex * 2;
        //     vertex.SetUV1(Vec2f(texcoord1Data[base], 1.0f - texcoord1Data[base + 1]));
        // }

        if (hasSkinning)
        {
            const cgltf_size base = vertexIndex * 4;

            for (uint32 i = 0; i < 4; ++i)
            {
                uint32 boneIndex = uint32(jointsFloatData[base + i]);
                float weight = weightsData[base + i];

                // Remap glTF joint indices (indices into the skin's joint list)
                // to bone indices of the engine skeleton
                if (jointRemap != nullptr)
                {
                    if (boneIndex < jointRemap->Size())
                    {
                        boneIndex = (*jointRemap)[boneIndex];
                    }
                    else
                    {
                        if (!ctx.loggedJointIndexOutOfRangeWarning)
                        {
                            ctx.loggedJointIndexOutOfRangeWarning = true;
                            HYP_LOG(Assets, Warning, "GLTF joint index out of range of the skin's joint list; the influence will be dropped");
                        }

                        boneIndex = 0;
                        weight = 0.0f;
                    }
                }

                vertex.SetBoneIndex(i, uint8(boneIndex));
                vertex.SetBoneWeight(i, weight);
            }
        }

        vertices[vertexIndex] = vertex;
    }

    ArborTreeWind wind;
    const bool hasWind = hasTexcoord1
        && texcoord1Data.Size() >= vertexCount * 2
        && ReadArborTreeWind(ctx, primitive, wind);

    const bool hasFoliage = hasWind && wind.leafOrigins.Size() >= vertexCount * 3;

    // the wind needs a tree's foot at the origin, so trees are not centered; skinned meshes aren't either,
    // since bone matrices rotate vertices about the mesh node's origin
    if (!hasWind && !hasSkinning && bounds.IsValid() && bounds.IsFinite() && !bounds.IsZero())
    {
        const Vec3f center = bounds.GetCenter();

        // offset vertices so that the mesh is centered around the origin
        for (FatVertex& vertex : vertices)
        {
            vertex.SetPosition(vertex.GetPosition() - center);
        }

        out.localTranslation = center;
    }

    Array<TreeVertex> treeVertices;
    Array<FoliageVertex> foliageVertices;

    if (hasFoliage)
    {
        BuildTreeVertices(wind, vertices, texcoord1Data, foliageVertices);
    }
    else if (hasWind)
    {
        BuildTreeVertices(wind, vertices, texcoord1Data, treeVertices);
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

        for (uint32 index : indices)
        {
            if (index >= vertexCount)
            {
                HYP_LOG(Assets, Warning, "GLTF primitive skipped: index {} out of range of {} vertices on mesh '{}'",
                        index, vertexCount, gltfMesh.name ? gltfMesh.name : "<unnamed>");
                return false;
            }
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
    if (hasFoliage)
    {
        meshDesc.meshAttributes.inputLayout = VertexInputLayoutDesc { VT_Simple | VT_Tree | VT_Foliage };
    }
    else if (hasWind)
    {
        meshDesc.meshAttributes.inputLayout = VertexInputLayoutDesc { VT_Simple | VT_Tree };
    }
    else
    {
        meshDesc.meshAttributes.inputLayout = VertexInputLayoutDesc { VT_Simple | VT_UV1 | VT_Skeletal };
    }

    meshDesc.meshAttributes.topology = topology;
    meshDesc.meshAttributes.indexBufferElemType = GpuElemType::UnsignedInt;
    meshDesc.lods[0].numVertices = uint32(vertices.Size());
    meshDesc.lods[0].numIndices = uint32(indices.Size());

    const Name assetName = MakePrimitiveName(gltfMesh, meshIndex, primitiveIndex);

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(assetName);

    VertexArrayView vertexArrayView {};

    if (hasFoliage)
    {
        vertexArrayView.floatData = reinterpret_cast<const float*>(foliageVertices.Data());
    }
    else if (hasWind)
    {
        vertexArrayView.floatData = reinterpret_cast<const float*>(treeVertices.Data());
    }
    else
    {
        vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    }

    vertexArrayView.vertexCount = vertices.Size();
    vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = indices.ToByteView();

    mesh->SetMeshData(meshDesc, meshData);

    if (!hasNormals && meshDesc.meshAttributes.topology == Topology::Triangles)
    {
        mesh->CalculateNormals();
    }

    if (ShouldRegisterAssets(ctx.state))
    {
        GetCurrentAssetRegistry()->PutAssetUnique(mesh);
    }

    // mesh->SetOriginalFilepath(FilePath::Relative(ctx.state.filepath, ctx.state.assetManager->GetBasePath()));
    InitObject(mesh);

    out.mesh = mesh;
    out.skinned = hasSkinning && !hasWind;
    out.tree = hasWind;
    out.foliage = hasFoliage;

    if (hasWind)
    {
        out.windFrequency = wind.frequency;
        out.windTrunkFlexibility = wind.flexibility[0];
        out.windTreeHeight = wind.height;
        out.windFlutter = wind.flutter;
    }

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
        return CreateNameFromDynamicString(ANSIString(StringUtil::StripExtension(basename)));
    }

    return NAME("GLTFModel");
}

Handle<Node> BuildNodeRecursive(GltfLoadContext& ctx, const cgltf_node& node)
{
    const Name nodeName = MakeNodeName(ctx, node);

    Handle<Node> nodeHandle = MakeHandle<Node>(nodeName);
    nodeHandle->SetLocalTransform(BuildTransformFromNode(node));

    Handle<Skeleton> nodeSkeleton;

    if (node.skin != nullptr)
    {
        const auto skinIt = ctx.skinResources.Find(node.skin);

        if (skinIt != ctx.skinResources.End() && skinIt->second.skeleton.IsValid())
        {
            nodeSkeleton = skinIt->second.skeleton;
        }
        else
        {
            HYP_LOG(Assets, Warning, "GLTF node '{}' references a skin that could not be loaded; its meshes will be static", nodeName);
        }
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

                Handle<Skeleton> skeleton;

                if (nodeSkeleton.IsValid())
                {
                    if (primitive.skinned)
                    {
                        skeleton = nodeSkeleton;
                    }
                    else if (!ctx.loggedSkinnedPrimitiveWithoutInfluencesWarning)
                    {
                        ctx.loggedSkinnedPrimitiveWithoutInfluencesWarning = true;
                        HYP_LOG(Assets, Warning, "GLTF primitive '{}' belongs to a skinned node but has no JOINTS_0/WEIGHTS_0 attributes; it will be rendered statically",
                                entityName);
                    }
                }

                ctx.scene->GetEntityManager()->AddComponent<MeshComponent>(entity, MeshComponent { primitive.mesh, primitive.material, skeleton });

                if (skeleton.IsValid())
                {
                    entity->SetIsDynamic(true);

                    AnimationComponent animationComponent {};
                    animationComponent.playbackState = {
                        .animationIndex = 0,
                        .status = AnimationPlaybackStatus::PLAYING,
                        .loopMode = AnimationLoopMode::REPEAT,
                        .speed = 1.0f,
                        .currentTime = 0.0f
                    };

                    ctx.scene->GetEntityManager()->AddComponent<AnimationComponent>(entity, animationComponent);
                }

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
    Scene& scene = GetDetachedSceneForCurrentThread();

    GltfLoadContext ctx {
        state,
        data,
        &scene
    };

    ctx.meshResources.Resize(data.meshes_count);

    for (cgltf_size skinIndex = 0; skinIndex < data.skins_count; ++skinIndex)
    {
        ctx.skinResources.Set(&data.skins[skinIndex], BuildSkinResource(ctx, data.skins[skinIndex]));
    }

    {
        Map<const cgltf_skin*, Array<Handle<Animation>>> animationsBySkin;

        for (cgltf_size animationIndex = 0; animationIndex < data.animations_count; ++animationIndex)
        {
            const cgltf_animation& gltfAnimation = data.animations[animationIndex];

            // Determine which skin this animation belongs to by looking at the first channel targeting a joint
            const cgltf_skin* targetSkin = nullptr;

            for (cgltf_size channelIndex = 0; channelIndex < gltfAnimation.channels_count; ++channelIndex)
            {
                const cgltf_animation_channel& channel = gltfAnimation.channels[channelIndex];

                if (channel.target_node == nullptr)
                {
                    continue;
                }

                for (const auto& skinIt : ctx.skinResources)
                {
                    if (skinIt.second.jointNodes.Contains(channel.target_node))
                    {
                        targetSkin = skinIt.first;

                        break;
                    }
                }

                if (targetSkin != nullptr)
                {
                    break;
                }
            }

            if (targetSkin == nullptr)
            {
                continue;
            }

            const auto skinIt = ctx.skinResources.Find(targetSkin);

            if (skinIt == ctx.skinResources.End() || !skinIt->second.skeleton.IsValid())
            {
                continue;
            }

            Handle<Animation> animation = BuildAnimationForSkin(state, gltfAnimation, uint32(animationIndex), skinIt->second);

            if (animation.IsValid() && animation->NumTracks() > 0)
            {
                if (ShouldRegisterAssets(state))
                {
                    GetCurrentAssetRegistry()->PutAssetUnique(animation);
                }

                animationsBySkin[targetSkin].PushBack(animation);
            }
        }

        for (const auto& animationIt : animationsBySkin)
        {
            if (const auto skinIt = ctx.skinResources.Find(animationIt.first); skinIt != ctx.skinResources.End() && skinIt->second.skeleton.IsValid())
            {
                skinIt->second.skeleton->SetAnimations(animationIt.second);
            }
        }
    }

    // Associate each mesh with the skin of the first node that instantiates it with a skin
    Map<const cgltf_mesh*, const cgltf_skin*> meshSkins;

    for (cgltf_size nodeIndex = 0; nodeIndex < data.nodes_count; ++nodeIndex)
    {
        const cgltf_node& node = data.nodes[nodeIndex];

        if (node.mesh != nullptr && node.skin != nullptr && meshSkins.Find(node.mesh) == meshSkins.End())
        {
            meshSkins.Set(node.mesh, node.skin);
        }
    }

    for (cgltf_size meshIndex = 0; meshIndex < data.meshes_count; ++meshIndex)
    {
        const cgltf_mesh& gltfMesh = data.meshes[meshIndex];
        GltfMeshResource& meshResource = ctx.meshResources[meshIndex];
        meshResource.primitives.Reserve(gltfMesh.primitives_count);

        const Array<uint32>* jointRemap = nullptr;

        if (const auto meshSkinIt = meshSkins.Find(&gltfMesh); meshSkinIt != meshSkins.End())
        {
            if (const auto skinIt = ctx.skinResources.Find(meshSkinIt->second); skinIt != ctx.skinResources.End())
            {
                jointRemap = &skinIt->second.jointToBoneIndex;
            }
        }

        for (cgltf_size primitiveIndex = 0; primitiveIndex < gltfMesh.primitives_count; ++primitiveIndex)
        {
            PrimitiveBuildOutput output;

            if (!BuildPrimitive(ctx, gltfMesh, gltfMesh.primitives[primitiveIndex], uint32(meshIndex), uint32(primitiveIndex), jointRemap, output))
            {
                continue;
            }

            Handle<Material> material = AcquireMaterial(state, ctx, gltfMesh.primitives[primitiveIndex].material, output.mesh);

            {
                MaterialParameters parameters = material->GetParameters();
                parameters.foliage = output.foliage;
                parameters.windFrequency = output.windFrequency;
                parameters.windTrunkFlexibility = output.windTrunkFlexibility;
                parameters.windTreeHeight = output.windTreeHeight;
                parameters.windFlutter = output.windFlutter;

                if (parameters != material->GetParameters())
                {
                    material->SetParameters(parameters);
                }
            }

            InitObject(material);

            meshResource.primitives.PushBack(GltfPrimitiveResource {
                .mesh = output.mesh,
                .material = material,
                .localTranslation = output.localTranslation,
                .gltfPrimitiveIndex = uint32(primitiveIndex),
                .skinned = output.skinned });
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

    return LoadedAsset { MakeHandle<Prefab>(root->GetName(), root) };
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

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
    Map<const cgltf_node*, Transform> bindingTransforms;
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
    bool loggedEmbeddedTextureWarning = false;
    bool loggedTextureWrapModeWarning = false;
    bool loggedJointIndexOutOfRangeWarning = false;
    bool loggedSkinnedPrimitiveWithoutInfluencesWarning = false;
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
                    srgb ? AssetLoadHint::TextureLoader_LoadAsSRGB : AssetLoadHint::NoHint);
                textureResult.HasValue())
            {
                const Handle<Texture>& texture = textureResult->Result();
                Check(texture->Create());

                GetCurrentAssetRegistry()->PutAssetUnique(texture);

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

    GetCurrentAssetRegistry()->PutAssetUnique(textureHandle);

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
                       float(node.rotation[3]))
                       .Inverse();
        rotation.Normalize();
    }

    return Transform(translation, scale, rotation);
}

/*! \brief Same as BuildTransformFromNode, but mirrors the transform across the Z axis,
 *  matching the vertex position conversion done in BuildPrimitive (right-handed to left-handed).
 *  This is used for bone binding transforms and animation keyframes, which have to live in the
 *  same space as the (mirrored) skinned vertices. */
Transform BuildMirroredTransformFromNode(const cgltf_node& node)
{
    if (node.has_matrix)
    {
        Mat4f matrix(node.matrix);
        matrix = matrix.Transpose();

        // Conjugate with D = diag(1, 1, -1, 1) to mirror across Z: M' = D * M * D
        matrix[0][2] *= -1.0f;
        matrix[1][2] *= -1.0f;
        matrix[2][0] *= -1.0f;
        matrix[2][1] *= -1.0f;
        matrix[2][3] *= -1.0f;

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

    // Synthetic root bone so that multiple separate joint hierarchies can be represented by a single skeleton
    Handle<Bone> rootBone = MakeHandle<Bone>(NAME_FMT("{}_Root", skinName));

    Array<Name> usedBoneNames;
    Map<const cgltf_node*, Handle<Bone>> bonesByNode;
    Map<const cgltf_node*, const cgltf_node*> jointParents;

    uint32 unnamedJointCounter = 0;

    for (cgltf_size jointIndex = 0; jointIndex < skin.joints_count; ++jointIndex)
    {
        const cgltf_node* jointNode = skin.joints[jointIndex];

        if (jointNode == nullptr || bonesByNode.Find(jointNode) != bonesByNode.End())
        {
            continue;
        }

        // The binding transform is the joint's local transform, with any intermediate non-joint
        // nodes folded in, so that the resulting bone hierarchy consists of joints only
        Transform bindingTransform = BuildMirroredTransformFromNode(*jointNode);
        const cgltf_node* jointParent = nullptr;

        for (const cgltf_node* parentNode = jointNode->parent; parentNode != nullptr; parentNode = parentNode->parent)
        {
            if (resource.jointNodes.Contains(parentNode))
            {
                jointParent = parentNode;

                break;
            }

            bindingTransform = BuildMirroredTransformFromNode(*parentNode) * bindingTransform;
        }

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
        jointParents.Set(jointNode, jointParent);
        resource.bindingTransforms.Set(jointNode, bindingTransform);
        resource.boneNames.Set(jointNode, boneName);
    }

    for (const auto& boneIt : bonesByNode)
    {
        Handle<Bone> parentBone = rootBone;

        if (const cgltf_node* jointParent = jointParents.At(boneIt.first); jointParent != nullptr)
        {
            if (const auto parentIt = bonesByNode.Find(jointParent); parentIt != bonesByNode.End())
            {
                parentBone = parentIt->second;
            }
        }

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

        rootBonePtr->CalculateBoneRotation();
        rootBonePtr->CalculateBoneTranslation();

        rootBonePtr->StoreBindingPose();
        rootBonePtr->ClearPose();

        rootBonePtr->UpdateBoneTransform();
    }

    GetCurrentAssetRegistry()->PutAssetUnique(skeleton);
    InitObject(skeleton);

    resource.skeleton = skeleton;

    return resource;
}

Handle<Animation> BuildAnimationForSkin(const cgltf_animation& animation, uint32 animationIndex, const GltfSkinResource& skinResource)
{
    const Name animationName = (animation.name && *animation.name)
        ? CreateNameFromDynamicString(animation.name)
        : NAME_FMT("Animation{}", animationIndex);

    Handle<Animation> result = MakeHandle<Animation>(animationName);

    for (cgltf_size channelIndex = 0; channelIndex < animation.channels_count; ++channelIndex)
    {
        const cgltf_animation_channel& channel = animation.channels[channelIndex];

        if (channel.target_node == nullptr || channel.sampler == nullptr)
        {
            continue;
        }

        switch (channel.target_path)
        {
        case cgltf_animation_path_type_translation:
        case cgltf_animation_path_type_rotation:
        case cgltf_animation_path_type_scale:
            break;
        default:
            // Morph target weights are not supported; a warning is emitted when loading mesh data
            continue;
        }

        if (!skinResource.jointNodes.Contains(channel.target_node))
        {
            // Only bones can be animated; channels targeting other nodes are ignored
            continue;
        }

        const cgltf_animation_sampler& sampler = *channel.sampler;

        Array<float> timesData;

        if (UnpackAccessorFloats(sampler.input, timesData) == 0)
        {
            HYP_LOG(Assets, Warning, "GLTF animation '{}' channel {} has no usable time data; skipping channel",
                    animationName, uint32(channelIndex));

            continue;
        }

        Array<float> valuesData;

        if (UnpackAccessorFloats(sampler.output, valuesData) == 0)
        {
            HYP_LOG(Assets, Warning, "GLTF animation '{}' channel {} has no usable output data; skipping channel",
                    animationName, uint32(channelIndex));

            continue;
        }

        const uint32 numComponents = channel.target_path == cgltf_animation_path_type_rotation ? 4 : 3;

        // Cubic spline samplers store an in-tangent, the value and an out-tangent per keyframe;
        // only the value (the middle element) is used, other interpolation types are treated as linear
        const cgltf_size componentStride = numComponents * (sampler.interpolation == cgltf_interpolation_type_cubic_spline ? 3 : 1);

        if (valuesData.Size() < timesData.Size() * componentStride)
        {
            HYP_LOG(Assets, Warning, "GLTF animation '{}' channel {} has an insufficient number of output values; skipping channel",
                    animationName, uint32(channelIndex));

            continue;
        }

        const auto bindingIt = skinResource.bindingTransforms.Find(channel.target_node);
        const Transform bindingTransform = bindingIt != skinResource.bindingTransforms.End()
            ? bindingIt->second
            : Transform();

        const auto boneNameIt = skinResource.boneNames.Find(channel.target_node);
        const Name boneName = boneNameIt != skinResource.boneNames.End() ? boneNameIt->second : Name::Invalid();

        Array<Keyframe> keyframes;
        keyframes.Resize(timesData.Size());

        for (cgltf_size keyframeIndex = 0; keyframeIndex < timesData.Size(); ++keyframeIndex)
        {
            // Non-animated components fall back to the bone's binding transform
            Vec3f translation = bindingTransform.GetTranslation();
            Vec3f scale = bindingTransform.GetScale();
            Quat4f rotation = bindingTransform.GetRotation();

            const cgltf_size base = keyframeIndex * componentStride
                + (sampler.interpolation == cgltf_interpolation_type_cubic_spline ? numComponents : 0);

            switch (channel.target_path)
            {
            case cgltf_animation_path_type_translation:
                translation = Vec3f(valuesData[base], valuesData[base + 1], -valuesData[base + 2]);
                break;
            case cgltf_animation_path_type_scale:
                scale = Vec3f(valuesData[base], valuesData[base + 1], valuesData[base + 2]);
                break;
            case cgltf_animation_path_type_rotation:
                rotation = Quat4f(
                               -valuesData[base],
                               -valuesData[base + 1],
                               valuesData[base + 2],
                               valuesData[base + 3])
                               .Inverse();
                rotation.Normalize();
                break;
            default:
                break;
            }

            keyframes[keyframeIndex] = Keyframe(timesData[keyframeIndex], Transform(translation, scale, rotation));
        }

        Handle<AnimationTrack> track = MakeHandle<AnimationTrack>(
            NAME_FMT("{}_{}", animationName, boneName),
            boneName);

        track->SetKeyframes(keyframes);

        GetCurrentAssetRegistry()->PutAssetUnique(track);

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

    GetCurrentAssetRegistry()->PutAssetUnique(roughnessTexture);

    Check(roughnessTexture->Create());

    Handle<Texture> metalnessTexture = MakeHandle<Texture>(metalnessDesc, metalnessData.ToByteView());
    metalnessTexture->SetName(NAME_FMT("{}_Metalness", baseName));

    GetCurrentAssetRegistry()->PutAssetUnique(metalnessTexture);

    Check(metalnessTexture->Create());

    return { metalnessTexture, roughnessTexture };
}

Handle<Material> AcquireMaterial(GltfLoadContext& ctx, const cgltf_material* gltfMaterial, const Handle<Mesh>& mesh)
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

        GetCurrentAssetRegistry()->PutAsset(fallbackMaterial);

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

    switch (gltfMaterial->alpha_mode)
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

    if (gltfMaterial->double_sided)
    {
        materialAttributes.cullFaces = FCM_NONE;
    }

    if (Handle<Texture> normalTexture = AcquireTexture(ctx, gltfMaterial->normal_texture, false); normalTexture.IsValid())
    {
        textures[MaterialTextureKey::Normals] = normalTexture;
    }

    if (Handle<Texture> occlusionTexture = AcquireTexture(ctx, gltfMaterial->occlusion_texture, false); occlusionTexture.IsValid())
    {
        textures[MaterialTextureKey::AmbientOcclusion] = occlusionTexture;
    }

    const Vec3f emissiveFactor(
        float(gltfMaterial->emissive_factor[0]),
        float(gltfMaterial->emissive_factor[1]),
        float(gltfMaterial->emissive_factor[2]));

    if (emissiveFactor != Vec3f::Zero())
    {
        parameters.emissiveIntensity = emissiveFactor.Length();
        parameters.emissiveColor = Color(Vec4f(emissiveFactor / parameters.emissiveIntensity, 1.0f));
    }

    Handle<Material> material = MakeHandle<Material>(
        materialName,
        materialAttributes,
        parameters,
        textures);

    GetCurrentAssetRegistry()->PutAsset(material);
    InitObject(material);

    ctx.materialCache.Set(gltfMaterial, material);

    return material;
}

struct PrimitiveBuildOutput
{
    Handle<Mesh> mesh;
    Vec3f localTranslation = Vec3f::Zero();
    bool skinned = false;
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

    if (bounds.IsValid() && bounds.IsFinite() && !bounds.IsZero())
    {
        const Vec3f center = bounds.GetCenter();

        // offset vertices so that the mesh is centered around the origin
        for (FatVertex& vertex : vertices)
        {
            vertex.SetPosition(vertex.GetPosition() - center);
        }

        out.localTranslation = center;
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
    meshDesc.lods[0].numVertices = uint32(vertices.Size());
    meshDesc.lods[0].numIndices = uint32(indices.Size());

    const Name assetName = MakePrimitiveName(gltfMesh, meshIndex, primitiveIndex);

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(assetName);

    VertexArrayView vertexArrayView {};
    vertexArrayView.floatData = reinterpret_cast<const float*>(vertices.Data());
    vertexArrayView.vertexCount = vertices.Size();
    vertexArrayView.layoutDesc = meshDesc.meshAttributes.inputLayout;

    MeshDataView meshData {};
    meshData.vertices[0] = vertexArrayView;
    meshData.indices[0] = indices.ToByteView();

    mesh->SetMeshData(meshDesc, meshData);

    if (!hasNormals && meshDesc.meshAttributes.topology == TOP_TRIANGLES)
    {
        mesh->CalculateNormals();
    }

    // mesh->SetOriginalFilepath(FilePath::Relative(ctx.state.filepath, ctx.state.assetManager->GetBasePath()));
    InitObject(mesh);

    GetCurrentAssetRegistry()->PutAssetUnique(mesh);

    out.mesh = mesh;
    out.skinned = hasSkinning;

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

                    if (!skeleton->GetAnimations().Empty())
                    {
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
    Scene* scene = GetDetachedSceneForCurrentThread();

    GltfLoadContext ctx {
        .state = state,
        .data = data,
        .scene = scene
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

            Handle<Animation> animation = BuildAnimationForSkin(gltfAnimation, uint32(animationIndex), skinIt->second);

            if (animation.IsValid() && animation->NumTracks() > 0)
            {
                GetCurrentAssetRegistry()->PutAssetUnique(animation);

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

            Handle<Material> material = AcquireMaterial(ctx, gltfMesh.primitives[primitiveIndex].material, output.mesh);
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

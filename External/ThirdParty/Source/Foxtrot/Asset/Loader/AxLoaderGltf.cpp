#include "AxLoaderGltf.hpp"

#include <ThirdParty/cgltf.h>

#include <Asset/AxBase.hpp>
#include <Asset/AxManager.hpp>
#include <Core/FxRef.hpp>
#include <FxMaterial.hpp>
#include <FxObject.hpp>
#include <Renderer/FxPrimitiveMesh.hpp>

// Renderer includes
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

FxTSRef<AxImage> LoadTexture(const FxTSRef<FxMaterial>& material, const cgltf_texture_view& texture_view);

void AxLoaderGltf::UnpackMeshAttributes(const FxTSRef<FxObject>& object, FxRef<FxPrimitiveMesh>& mesh,
                                        cgltf_primitive* primitive)
{
    FxSizedArray<float32> positions;
    FxSizedArray<float32> normals;
    FxSizedArray<float32> uvs;
    FxSizedArray<float32> tangents;
    FxSizedArray<float32> weights;
    FxSizedArray<uint32> boneids;

    for (int i = 0; i < primitive->attributes_count; i++) {
        auto* attribute = &primitive->attributes[i];

        if (attribute->type == cgltf_attribute_type_position) {
            cgltf_size data_size = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
            positions.InitSize(data_size);
            cgltf_accessor_unpack_floats(attribute->data, positions.pData, data_size);
        }
        else if (attribute->type == cgltf_attribute_type_normal) {
            cgltf_size data_size = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
            normals.InitSize(data_size);
            cgltf_accessor_unpack_floats(attribute->data, normals.pData, data_size);
        }
        else if (attribute->type == cgltf_attribute_type_texcoord) {
            cgltf_size data_size = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
            uvs.InitSize(data_size);
            cgltf_accessor_unpack_floats(attribute->data, uvs.pData, data_size);
        }
        else if (attribute->type == cgltf_attribute_type_tangent) {
            cgltf_size data_size = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
            tangents.InitSize(data_size);
            cgltf_accessor_unpack_floats(attribute->data, tangents.pData, data_size);
        }
        else if (attribute->type == cgltf_attribute_type_weights) {
            cgltf_size data_size = cgltf_accessor_unpack_floats(attribute->data, nullptr, 0);
            weights.InitSize(data_size);
            cgltf_accessor_unpack_floats(attribute->data, weights.pData, data_size);
        }
        else if (attribute->type == cgltf_attribute_type_joints) {
            boneids.InitSize(attribute->data->count * 4);

            for (cgltf_size j = 0; j < attribute->data->count; j++) {
                cgltf_accessor_read_uint(attribute->data, j, reinterpret_cast<cgltf_uint*>(&boneids.pData[j * 4]), 4);
            }
        }
    }

    mesh->VertexList.CreateFrom(positions, normals, uvs, tangents, weights, boneids);

    FxLogInfo("Weights size: {}, ids size: {}", weights.Size, boneids.Size);
    mesh->UploadVertices();
}

void AxLoaderGltf::LoadAnimationSkin(FxRef<FxPrimitiveMesh>& mesh, cgltf_skin* skin)
{
    FxLogInfo("Joints: {}", skin->joints_count);
}

template <RxImageFormat TFormat>
static void MakeMaterialTextureForPrimitive(FxTSRef<FxMaterial>& material, FxMaterialComponent<TFormat>& component,
                                            cgltf_texture_view& texture_view)
{
    FxAssert(texture_view.texture != nullptr);

    const uint8* image_buffer = cgltf_buffer_view_data(texture_view.texture->image->buffer_view);
    uint32 image_buffer_size = static_cast<uint32>(texture_view.texture->image->buffer_view->size);

    // Stage that shit so we can nuke mGltfData as soon as we can
    uint8* goober_buffer = gEnginePool->Alloc<uint8>(image_buffer_size);
    memcpy(goober_buffer, image_buffer, image_buffer_size);

    // Submit as data to be loaded later by the asset manager
    component.pDataToLoad = FxMakeSlice(const_cast<const uint8*>(goober_buffer), image_buffer_size);
}

void AxLoaderGltf::MakeMaterialForPrimitive(FxTSRef<FxObject>& object, cgltf_primitive* primitive)
{
    cgltf_material* gltf_material = primitive->material;

    if (!gltf_material) {
        return;
    }

    FxTSRef<FxMaterial> material = gMaterialManager->New(object->Name.Get(), &gRenderer->pDeferredRenderer->PlGeometry,
                                                         object->IsSkinned());

    // For some reason the peeber metallic roughness holds our diffuse texture
    if (gltf_material->has_pbr_metallic_roughness) {
        cgltf_texture_view& texture_view = gltf_material->pbr_metallic_roughness.base_color_texture;

        if (!texture_view.texture) {
            // MakeEmptyMaterialTexture(material, material->Diffuse);
            material->Diffuse.pAssetImage = AxImage::GetEmptyImage<RxImageFormat::eRGBA8_UNorm>();

            material->Properties.BaseColor = FxColor::FromFloats(
                gltf_material->pbr_metallic_roughness.base_color_factor);
        }
        else {
            MakeMaterialTextureForPrimitive(material, material->Diffuse, texture_view);
            material->Properties.BaseColor = FxColor::FromRGBA(1, 1, 1, 255);
        }
    }
    else {
        // There is no albedo texture on the model, use the base colour.
        material->Properties.BaseColor = FxColor::FromFloats(gltf_material->pbr_metallic_roughness.base_color_factor);
    }

    // Load the normalmap
    if (gltf_material->normal_texture.texture != nullptr) {
        MakeMaterialTextureForPrimitive(material, material->NormalMap, gltf_material->normal_texture);
    }

    // Load the metallic/roughness texture
    if (gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr) {
        MakeMaterialTextureForPrimitive(material, material->MetallicRoughness,
                                        gltf_material->pbr_metallic_roughness.metallic_roughness_texture);
    }


    object->pMaterial = material;
}

void AxLoaderGltf::UploadMeshToGpu(FxTSRef<FxObject>& object, cgltf_mesh* gltf_mesh, int mesh_index)
{
    const bool has_multiple_primitives = gltf_mesh->primitives_count > 1;

    // Assume at first that there is only one primitive;
    FxTSRef<FxObject> current_object = object;

    // Similarly to `CreateGpuResource`, we are going to make the `object` into a container
    // if there are multiple primitives.
    if (has_multiple_primitives) {
        current_object = FxTSRef<FxObject>::New();
    }

    for (int i = 0; i < gltf_mesh->primitives_count; i++) {
        cgltf_primitive* primitive = &gltf_mesh->primitives[i];

        FxSizedArray<uint32> indices;

        FxRef<FxPrimitiveMesh> primitive_mesh = FxRef<FxPrimitiveMesh>::New();

        // Keep the primitive mesh's vertices and indices in memory if `KeepInMemory` is set
        primitive_mesh->bKeepInMemory = bKeepInMemory;

        // if there are indices in the mesh, add them to the FxPrimitiveMesh
        if (primitive->indices != nullptr) {
            indices.InitSize(primitive->indices->count);

            cgltf_accessor_unpack_indices(primitive->indices, indices.pData, sizeof(uint32), primitive->indices->count);

            // Set the mesh indices
            primitive_mesh->UploadIndices(std::move(indices));
        }

        UnpackMeshAttributes(current_object, primitive_mesh, primitive);
        current_object->pMesh = primitive_mesh;

        MakeMaterialForPrimitive(current_object, primitive);

        primitive_mesh->bIsReady = true;


        if (has_multiple_primitives) {
            // Attach the current object to the object container (our output)
            object->AttachObject(current_object);

            // Create a new object to load into next
            current_object = FxTSRef<FxObject>::New();
        }
    }

    //    object = current_object;
}


int find_keyframe_index(cgltf_accessor* input, float t)
{
    for (size_t i = 0; i < input->count - 1; ++i) {
        float t0, t1;
        cgltf_accessor_read_float(input, i, &t0, 1);
        cgltf_accessor_read_float(input, i + 1, &t1, 1);
        if (t >= t0 && t < t1)
            return i;
    }
    return 0;
}

void AxLoaderGltf::LoadAnimations()
{
    float t = 1.0;


    if (!mpGltfData->animations_count) {
        return;
    }

    FxQuat rotation = FxQuat::sIdentity;
    FxVec3f translation = FxVec3f::sZero;

    cgltf_animation& anim = mpGltfData->animations[0];

    for (size_t i = 0; i < anim.channels_count; ++i) {
        cgltf_animation_channel* channel = &anim.channels[i];
        cgltf_animation_sampler* sampler = channel->sampler;

        // Find keyframe interval
        int idx = find_keyframe_index(sampler->input, t);
        float t0, t1;
        cgltf_accessor_read_float(sampler->input, idx, &t0, 1);
        cgltf_accessor_read_float(sampler->input, idx + 1, &t1, 1);
        float alpha = (t - t0) / (t1 - t0);

        // Interpolate based on property type
        if (channel->target_path == cgltf_animation_path_type_translation) {
            FxVec3f v0, v1, result;
            cgltf_accessor_read_float(sampler->output, idx, v0.mData, 3);
            cgltf_accessor_read_float(sampler->output, idx + 1, v1.mData, 3);

            translation = v0.LerpIP(v1, alpha);
        }
        else if (channel->target_path == cgltf_animation_path_type_rotation) {
            float buffer[4];

            cgltf_accessor_read_float(sampler->output, idx, buffer, 4);
            FxQuat q0(buffer);

            cgltf_accessor_read_float(sampler->output, idx + 1, buffer, 4);
            FxQuat q1(buffer);

            rotation = q0.SLerp(q1, alpha);
        }
    }

    FxMat4f xform_matrix = FxMat4f::AsRotation(rotation) * FxMat4f::AsTranslation(translation);

    // for (uint32 index = 0; index < mpGltfData->animations_count; index++) {
    //     FxLogInfo("\tAnimation {} has {} samplers and {} channels", index, anim.samplers_count, anim.channels_count);
    // }


    FxLogInfo("Gltf model has {} animations", mpGltfData->animations_count);
}


AxLoaderGltf::Status AxLoaderGltf::LoadFromFile(FxTSRef<AxBase> asset, const std::string& path)
{
    cgltf_options options {};

    cgltf_result status = cgltf_parse_file(&options, path.c_str(), &mpGltfData);
    if (status != cgltf_result_success) {
        FxLogError("Error parsing GLTF file! (path: {:s})", path);
        return AxLoaderGltf::Status::eError;
    }

    status = cgltf_load_buffers(&options, mpGltfData, path.c_str());
    if (status != cgltf_result_success) {
        FxLogError("Error loading buffers from GLTF file! (path: {:s})", path);

        return AxLoaderGltf::Status::eError;
    }


    return AxLoaderGltf::Status::eSuccess;
}

AxLoaderGltf::Status AxLoaderGltf::LoadFromMemory(FxTSRef<AxBase> asset, const uint8* data, uint32 size)
{
    // (void)asset;
    //    FxRef<FxAssetModel> model(asset);

    cgltf_options options {};

    cgltf_result status = cgltf_parse(&options, data, size, &mpGltfData);
    if (status != cgltf_result_success) {
        FxLogError("Error parsing GLTF file from data");
        return AxLoaderGltf::Status::eError;
    }

    return AxLoaderGltf::Status::eSuccess;
}

void AxLoaderGltf::CreateGpuResource(FxTSRef<AxBase>& asset)
{
    FxTSRef<FxObject> output_object(asset);


    // If there is only one mesh to load, store the mesh directly in the output object
    FxTSRef<FxObject> current_object = output_object;

    // If there are multiple gltf meshes, we will need to use the output object as a
    // container for multiple other meshes
    const bool has_multiple_meshes = mpGltfData->meshes_count > 1;
    if (has_multiple_meshes) {
        current_object = FxTSRef<FxObject>::New();
    }


    LoadAnimations();


    FxLogInfo("Unpacking GLTF object with {} meshes", mpGltfData->meshes_count);

    for (int32 node_index = 0; node_index < mpGltfData->nodes_count; node_index++) {
        cgltf_node* node = &mpGltfData->nodes[node_index];

        // Might be good to not ignore the other GLTF things in the future, but thats not a priority for now
        if (!node->mesh) {
            continue;
        }

        UploadMeshToGpu(current_object, node->mesh, node_index);

        if (node->skin) {
            LoadAnimationSkin(current_object->pMesh, node->skin);
        }

        if (has_multiple_meshes) {
            // Attach the current object to the object container (our output)
            output_object->AttachObject(current_object);

            // Create a new object to load into next
            current_object = FxTSRef<FxObject>::New();
        }
    }

    asset = output_object;

    asset->bIsUploadedToGpu = true;
    asset->bIsUploadedToGpu.notify_all();
}

void AxLoaderGltf::Destroy(FxTSRef<AxBase>& asset)
{
    if (mpGltfData) {
        cgltf_free(mpGltfData);
        mpGltfData = nullptr;
    }
}

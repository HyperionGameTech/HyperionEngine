#pragma once

#include "AxLoaderBase.hpp"

#include <FxMaterial.hpp>
#include <FxObject.hpp>
#include <string>

struct cgltf_data;
struct cgltf_material;
struct cgltf_mesh;
struct cgltf_texture_view;
struct cgltf_primitive;
struct cgltf_skin;

struct AxGltfMaterialToLoad
{
    FxTSRef<FxObject> pObject { nullptr };
    int PrimitiveIndex = 0;
    int MeshIndex = 0;
};

class AxLoaderGltf : public AxLoaderBase
{
public:
    using Status = AxLoaderBase::Status;

    AxLoaderGltf() = default;

    Status LoadFromFile(FxTSRef<AxBase> asset, const std::string& path) override;
    Status LoadFromMemory(FxTSRef<AxBase> asset, const uint8* data, uint32 size) override;

    void UploadMeshToGpu(FxTSRef<FxObject>& object, cgltf_mesh* gltf_mesh, int mesh_index);

    void Destroy(FxTSRef<AxBase>& asset) override;

    ~AxLoaderGltf() override = default;

private:
    // void MakeEmptyMaterialTexture(FxRef<FxMaterial>& material, FxMaterialComponent& component);
    void MakeMaterialForPrimitive(FxTSRef<FxObject>& object, cgltf_primitive* primitive);

    void UnpackMeshAttributes(const FxTSRef<FxObject>& object, FxRef<FxPrimitiveMesh>& mesh,
                              cgltf_primitive* primitive);

    void LoadAnimations();
    void LoadAnimationSkin(FxRef<FxPrimitiveMesh>& mesh, cgltf_skin* skin);

public:
    std::vector<AxGltfMaterialToLoad> MaterialsToLoad;

    bool bKeepInMemory : 1 = false;

    FxSizedArray<uint32> IndexBuffer;

protected:
    void CreateGpuResource(FxTSRef<AxBase>& asset) override;

private:
    cgltf_data* mpGltfData = nullptr;

    FxSizedArray<FxMat4f> mBones;
};

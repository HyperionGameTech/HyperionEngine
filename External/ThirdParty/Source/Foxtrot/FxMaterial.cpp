#include "FxMaterial.hpp"

#include <vulkan/vulkan.h>

#include <Asset/AxManager.hpp>
#include <Core/FxDefines.hpp>
#include <Core/FxStackArray.hpp>
#include <FxObjectManager.hpp>
#include <Renderer/Backend/RxCommands.hpp>
#include <Renderer/Backend/RxDevice.hpp>
#include <Renderer/Backend/RxPipeline.hpp>
#include <Renderer/RxDeferred.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

FX_SET_MODULE_NAME("FxMaterial")

void FxMaterialManager::Create(uint32 entities_per_page)
{
    if (mbInitialized) {
        return;
    }

    mMaterials.Create(entities_per_page);

    MaterialsInUse.InitZero(FX_MAX_MATERIALS);

    RxDescriptorPool& dp = mDescriptorPool;

    if (!dp.Pool) {
        dp.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20);
        dp.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4);
        dp.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 3);
        dp.Create(gRenderer->GetDevice(), FX_MAX_MATERIALS);
    }

    // Material properties buffer descriptors
    const uint32 material_buffer_size = FX_MAX_MATERIALS;

    MaterialPropertiesBuffer.Create(RxGpuBufferType::eStorage, sizeof(FxMaterialProperties) * material_buffer_size,
                                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, RxGpuBufferFlags::ePersistentMapped);


    if (!mMaterialPropertiesDS.IsInited()) {
        FxAssert(gRenderer->pDeferredRenderer->DsLayoutLightingMaterialProperties != nullptr);
        mMaterialPropertiesDS.Create(dp, gRenderer->pDeferredRenderer->DsLayoutLightingMaterialProperties, false);
    }

    mMaterialPropertiesDS.AddBuffer(0, &MaterialPropertiesBuffer, 0, VK_WHOLE_SIZE);
    mMaterialPropertiesDS.Build();

    RxUtil::SetDebugLabel("Material Properties DS", VK_OBJECT_TYPE_DESCRIPTOR_SET, mMaterialPropertiesDS.Get());

    mbInitialized = true;
}

FxTSRef<FxMaterial> FxMaterialManager::New(const std::string& name, RxPipeline* pipeline, bool supports_skinning)
{
    std::lock_guard guard(mInUse);

    if (!mMaterials.IsInited()) {
        Create();
    }

    uint32 free_material_index = MaterialsInUse.FindNextFreeBit();
    FxLogInfo("Free mat index: {}", free_material_index);
    FxAssert(free_material_index != FxBitset::scNoFreeBits);

    FxTSRef<FxMaterial> ref = FxTSRef<FxMaterial>::New();

    ref->Name = name;
    ref->pPipeline = pipeline;
    ref->mMaterialPropertiesIndex = free_material_index;
    ref->SetSupportsSkinning(supports_skinning);

    MaterialsInUse.Set(free_material_index);

    return ref;
}

void FxMaterialManager::Destroy()
{
    if (!mbInitialized) {
        return;
    }

    MaterialPropertiesBuffer.Destroy();

    mDescriptorPool.Destroy();

    mbInitialized = false;
}

#define CHECK_COMPONENT_READY(component_)                                                                              \
    if (component_.Exists() && !component_.pAssetImage->IsLoaded()) {                                                  \
        return false;                                                                                                  \
    }

bool FxMaterial::IsReady()
{
    if (mbIsReady) {
        return true;
    }

    if (!Diffuse.Exists()) {
        return false;
    }

    CHECK_COMPONENT_READY(Diffuse);
    CHECK_COMPONENT_READY(NormalMap);
    CHECK_COMPONENT_READY(MetallicRoughness);

    return (mbIsReady = true);
}

RxDescriptorSet& FxMaterial::GetDescriptorSetAlbedoOnly()
{
    if (mDsAlbedoOnly.IsInited()) {
        return mDsAlbedoOnly;
    }

    mDsAlbedoOnly.Create(gMaterialManager->GetDescriptorPool(),
                         gRenderer->pDeferredRenderer->DsLayoutGPassMaterialAlbedoOnly, false, 1);


    mDsAlbedoOnly.AddImage(0, &Diffuse.pAssetImage->Image, &gRenderer->Swapchain.ColorSampler);
    mDsAlbedoOnly.Build();

    return mDsAlbedoOnly;
}


bool FxMaterial::Bind(RxCommandBuffer* cmd) { return BindWithPipeline(*cmd, *pPipeline, false); }

bool FxMaterial::BindWithPipeline(RxCommandBuffer& cmd, RxPipeline& pipeline, bool albedo_only)
{
    if (!bIsBuilt.load()) {
        Build();
    }

    if (!IsReady() || !mDsDefault) {
        return false;
    }

    pipeline.Bind(cmd);

    VkDescriptorSet sets_to_bind[] = {
        mDsDefault.Get(),                              // Set 0: Textures (Albedo, Normal map, Metallic/Roughness)
        gMaterialManager->mMaterialPropertiesDS.Get(), // Set 1: Material Properties Buffer
    };

    FxStackArray<uint32, 1> offsets;
    if (bSupportsSkinning) {
        offsets.Insert(gRenderer->BoneBuffer.GetBaseOffset());
    }


    RxDescriptorSet::BindMultipleOffset(0, cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline,
                                        FxMakeSlice(sets_to_bind, std::size(sets_to_bind)), FxSlice<uint32>(offsets));

    return true;
}

void FxMaterial::Destroy()
{
    if (bIsBuilt) {
        bIsBuilt.store(false);
    }

    if (mMaterialPropertiesIndex != UINT32_MAX) {
        gMaterialManager->MaterialsInUse.Unset(mMaterialPropertiesIndex);
    }
}


template <RxImageFormat TFormat>
static bool CheckComponentTextureLoaded(FxMaterialComponent<TFormat>& component)
{
    if (!component.pAssetImage && component.pDataToLoad) {
        FxSlice<const uint8>& image_data = component.pDataToLoad;

        component.pAssetImage = gAssetManager->LoadImageFromMemory(component.Format, image_data.pData, image_data.Size);
        return false;
    }

    if (!component.pAssetImage || !component.pAssetImage.IsLoaded()) {
        return false;
    }

    return true;
}


#define BUILD_MATERIAL_COMPONENT(component_)                                                                           \
    {                                                                                                                  \
        if (component_.Build() == FxMaterialComponentStatus::eNotReady) {                                              \
            return;                                                                                                    \
        }                                                                                                              \
    }

void FxMaterial::Build()
{
    if (!mDsDefault.IsInited()) {
        VkDescriptorSetLayout layout = gRenderer->pDeferredRenderer->DsLayoutGPassMaterial;

        if (bSupportsSkinning) {
            layout = gRenderer->pDeferredRenderer->DsLayoutGPassSkinned;
        }

        mDsDefault.Create(gMaterialManager->GetDescriptorPool(), layout, false, 1);
    }

    // Build components
    BUILD_MATERIAL_COMPONENT(Diffuse);

    BUILD_MATERIAL_COMPONENT(NormalMap);

    // MetallicRoughness.Build();
    BUILD_MATERIAL_COMPONENT(MetallicRoughness);

    // Fill material descriptor

    mDsDefault.AddImage(0, &Diffuse.pAssetImage->Image, &gRenderer->Swapchain.ColorSampler);

    // When there is no normal map, we do not add it to the descriptor set. We should not bind extra garbage when we do
    // not need to.

    if (NormalMap.Exists()) {
        mDsDefault.AddImage(1, &NormalMap.pAssetImage->Image, &gRenderer->Swapchain.NormalsSampler);

        // To reduce permutations -- the metallic roughness map should only be
        // enabled if there is also a normal map.
        if (!MetallicRoughness.Exists()) {
            MetallicRoughness.pAssetImage = AxImage::GetEmptyImage<RxImageFormat::eRGBA8_SRGB>();
        }

        mDsDefault.AddImage(2, &MetallicRoughness.pAssetImage->Image, &gRenderer->Swapchain.ColorSampler);
    }

    if (bSupportsSkinning) {
        mDsDefault.AddBuffer(3, &gRenderer->BoneBuffer.GetGpuBuffer(), 0, gRenderer->BoneBuffer.Size);
    }

    mDsDefault.Build();

    FxAssert(mMaterialPropertiesIndex != UINT32_MAX);

    FxMaterialProperties* materials_buffer = static_cast<FxMaterialProperties*>(
        gMaterialManager->MaterialPropertiesBuffer.pMappedBuffer);

    FxMaterialProperties* material = &materials_buffer[mMaterialPropertiesIndex];
    material->BaseColor = Properties.BaseColor;

    if (NormalMap.Exists()) {
        pPipeline = &gRenderer->pDeferredRenderer->PlGeometryWithNormalMaps;
    }
    else {
        pPipeline = &gRenderer->pDeferredRenderer->PlGeometry;
    }

    if (bSupportsSkinning) {
        pPipeline = &gRenderer->pDeferredRenderer->PlGeometrySkinned;
    }

    bIsBuilt.store(true);
}

#pragma once

#include <vulkan/vulkan.h>

#include <Asset/AxImage.hpp>
#include <Asset/Fwd/Ax_Fwd_Manager.hpp>
#include <Core/FxBitset.hpp>
#include <Core/FxName.hpp>
#include <Core/FxPagedArray.hpp>
#include <FxColor.hpp>
#include <Renderer/Backend/RxDescriptors.hpp>
#include <Renderer/Backend/RxGpuBuffer.hpp>

#define FX_MAX_MATERIALS 128

enum class FxMaterialComponentStatus
{
    eReady,
    eMissingComponent,
    eNotReady,
};

/////////////////////////////////////
// Material Component
/////////////////////////////////////

template <RxImageFormat TFormat>
struct FxMaterialComponent
{
public:
public:
    FxTSRef<AxImage> pAssetImage { nullptr };
    FxSlice<const uint8> pDataToLoad { nullptr };

    using Status = FxMaterialComponentStatus;

public:
    FxMaterialComponent::Status Build()
    {
        // There is no texture provided, we will use the base colours passed in and a dummy texture
        if (!pAssetImage && !pDataToLoad) {
            // pAssetImage = AxImage::GetEmptyImage<TFormat>();
            return Status::eMissingComponent;
        }

        if (!CheckIfReady()) {
            // The texture is not ready, return the status to the material build function
            return Status::eNotReady;
        }

        return Status::eReady;
    }

    bool Exists() const { return (pAssetImage != nullptr); }

    ~FxMaterialComponent() = default;

private:
    bool CheckIfReady()
    {
        // If there is data passed in and the image has not been loaded yet, load it using the
        // asset manager. This will be validated on the next call of this function. (when attempting to build the
        // material)
        if (!pAssetImage && pDataToLoad) {
            FxSlice<const uint8>& image_data = pDataToLoad;
            pAssetImage = Fwd::AssetManager::LoadImageFromMemory(TFormat, image_data.pData, image_data.Size);

            return false;
        }

        // If there is no texture and we are not loaded, return not loaded.
        if (!pAssetImage || !pAssetImage->IsLoaded()) {
            return false;
        }

        return true;
    }
};

/////////////////////////////////////
// Material
/////////////////////////////////////

struct FxMaterialProperties
{
    alignas(16) FxColor BaseColor = 0xFFFFFFFFu;
};

class FxMaterial
{
public:
    enum ResourceType : uint32
    {
        eDiffuse,
        eNormal,
        eMetallicRoughness,

        eMaxImages,
    };

public:
    FxMaterial() = default;

    void Attach(ResourceType type, const FxTSRef<AxImage>& image)
    {
        switch (type) {
        case ResourceType::eDiffuse:
            Diffuse.pAssetImage = image;
            break;
        case ResourceType::eNormal:
            NormalMap.pAssetImage = image;
            break;
        case ResourceType::eMetallicRoughness:
            MetallicRoughness.pAssetImage = image;
            break;

        default:
            FxLogError("Unsupported resource type to attach to material!");
            break;
        }
    }

    bool IsReady();

    void SetSupportsSkinning(bool value) { bSupportsSkinning = value; }
    inline uint32 GetMaterialIndex() { return mMaterialPropertiesIndex; }

    /**
     * Binds the material to be used in the given command buffer.
     * @returns True if the material was bound succesfully.
     */
    bool Bind(RxCommandBuffer* cmd);
    bool BindWithPipeline(RxCommandBuffer& cmd, RxPipeline& pipeline, bool albedo_only = false);

    void Build();

    RxDescriptorSet& GetDescriptorSet() { return mDsDefault; }
    RxDescriptorSet& GetDescriptorSetAlbedoOnly();

    void Destroy();
    ~FxMaterial() { Destroy(); }

public:
    //    FxRef<FxAssetImage> DiffuseTexture{nullptr};
    FxMaterialComponent<RxImageFormat::eRGBA8_UNorm> Diffuse {};
    FxMaterialComponent<RxImageFormat::eRGBA8_UNorm> NormalMap {};
    FxMaterialComponent<RxImageFormat::eRGBA8_UNorm> MetallicRoughness {};

    FxMaterialProperties Properties {};

    FxName Name;

    RxPipeline* pPipeline = nullptr;

    std::atomic_bool bIsBuilt { false };

    bool bSupportsSkinning = false;

    /**
     * @brief Offset into `MaterialPropertiesBuffer` for this material.
     */
    uint32 mMaterialPropertiesIndex = UINT32_MAX;

private:
    RxDescriptorSet mDsDefault;
    RxDescriptorSet mDsAlbedoOnly;


    bool mbIsReady : 1 = false;
};

/////////////////////////////////////
// Material Manager
/////////////////////////////////////

class FxMaterialManager
{
public:
    void Create(uint32 materials_per_page = 64);
    FxTSRef<FxMaterial> New(const std::string& name, RxPipeline* pipeline, bool supports_skinning);

    RxDescriptorPool& GetDescriptorPool() { return mDescriptorPool; }

    void Destroy();

    ~FxMaterialManager() { Destroy(); }

    // RxRawGpuBuffer<FxMaterialProperties> MaterialPropertiesUbo;

    /**
     * @brief A large GPU buffer containing all loaded in material properties.
     */
    RxRawGpuBuffer MaterialPropertiesBuffer {};
    uint32 NumMaterialsInBuffer = 0;

    FxBitset MaterialsInUse;

    /**
     * @brief Descriptor set for material properties. Used in the light pass.
     */
    RxDescriptorSet mMaterialPropertiesDS {};

private:
    FxPagedArray<FxMaterial> mMaterials;
    RxDescriptorPool mDescriptorPool;

    bool mbInitialized : 1 = false;

    std::mutex mInUse;
};

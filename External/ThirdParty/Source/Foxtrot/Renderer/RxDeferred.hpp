#pragma once

#include "Backend/RxDescriptors.hpp"
#include "Backend/RxFrameData.hpp"
#include "Backend/RxFramebuffer.hpp"
#include "Backend/RxImage.hpp"
#include "Backend/RxSampler.hpp"
#include "RxPipelineList.hpp"
#include "RxRenderStage.hpp"

class FxCamera;

struct RxFrameData;

class RxDeferredCompPass;
class RxDeferredLightingPass;


///////////////////////////////
// Main Deferred Renderer
///////////////////////////////

class RxDeferredRenderer
{
public:
    void Create(const FxVec2u& extent);

    void DoCompPass(FxCamera& camera);

    void Destroy();
    ~RxDeferredRenderer() { Destroy(); }

private:
    // Geometry
    void CreateGPassPipeline();
    void DestroyGPassPipeline();

    void CreateGPass();

    VkPipelineLayout CreateGPassPipelineLayout();
    VkPipelineLayout CreateGPassSkinnedPipelineLayout();
    VkPipelineLayout CreateUnlitPipelineLayout();

    // Lighting
    // void CreateLightVolumePipeline();
    void CreateLightingPipeline();

    void CreateLightingDSLayout();

    // void DestroyLightVolumePipeline();
    void DestroyLightingPipeline();

    VkPipelineLayout CreateLightingPipelineLayout();

    // Composition
    void CreateCompPipeline();
    void DestroyCompPipeline();

    void CreateUnlitPipeline();

    VkPipelineLayout CreateCompPipelineLayout();

    void CreateCompPass();
    void BuildLightDescriptors();

public:
    RxDescriptorPool DescriptorPool;

    /////////////////////
    // Geometry Pass
    /////////////////////

    VkDescriptorSetLayout DsLayoutGPassMaterial = nullptr;
    VkDescriptorSetLayout DsLayoutGPassSkinned = nullptr;

    VkDescriptorSetLayout DsLayoutGPassMaterialAlbedoOnly = nullptr;

    RxRenderStage GPass;

    RxPipeline PlGeometry;
    RxPipeline PlGeometryNoDepthTest;
    RxPipeline PlGeometryWireframe;
    RxPipeline PlGeometryWithNormalMaps;

    RxPipeline PlGeometrySkinned;

    RxPipeline* pGeometryPipeline = nullptr;

    //////////////////////
    // Lighting Pass
    //////////////////////

    VkDescriptorSetLayout DsLayoutLightingFrag = nullptr;
    VkDescriptorSetLayout DsLayoutLightingMaterialProperties = nullptr;

    RxDescriptorSet DsLighting;

    RxRenderStage LightPass;

    RxPipeline PlLightingOutsideVolume;
    RxPipeline PlLightingInsideVolume;
    RxPipeline PlLightingDirectional;

    /////////////////////////////////////////////////
    // Forward pass / Unlit
    /////////////////////////////////////////////////
    // VkDescriptorSetLayout DsLayoutUnlit = nullptr;
    RxPipeline PlUnlit;
    // RxDescriptorSet DsUnlit;

    RxRenderPass RpForward;
    RxFramebuffer FbForward;

    //////////////////////
    // Composition Pass
    //////////////////////

    VkDescriptorSetLayout DsLayoutCompFrag = nullptr;

    RxDescriptorSet DsComposition;

    RxRenderStage CompPass;

    RxPipeline PlComposition;
    RxPipeline PlCompositionUnlit;
};

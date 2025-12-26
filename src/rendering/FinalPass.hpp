/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>
#include <core/reflection/ObjectBase.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/Types.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

namespace Hyperion {

class ShaderManager;
class FullScreenPass;
class Mesh;
class SwapchainBase;
struct RenderSetup;

struct FinalPassData
{
    SwapchainWeakRef swapchain;
    Handle<FullScreenPass> renderTextureToScreenPass;
    FixedArray<DescriptorSetRef, NumFramesInFlight> descriptorSets;
    GpuImageViewRef lastUiImageView;
    uint8 dirtyFrameIndices = 0;
};

class FinalPass final
{
public:
    FinalPass();
    FinalPass(const FinalPass& other) = delete;
    FinalPass& operator=(const FinalPass& other) = delete;
    ~FinalPass();

    void SetUILayerImageView(const GpuImageViewRef& imageView);

    void Create();
    void Render(Frame* frame, const RenderSetup& rs);

private:
    FinalPassData* GetOrCreatePassData(Swapchain* swapchain);

    SparsePagedArray<FinalPassData> m_passData;
    Handle<Mesh> m_quadMesh;
    GpuImageViewRef m_uiLayerImageView;
};
} // namespace Hyperion

/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_GLOBAL_STATE_HPP
#define HYPERION_RENDER_GLOBAL_STATE_HPP

#include <core/memory/UniquePtr.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/Bindless.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion {

class Engine;
class Entity;
class ShadowMapAllocator;
class GPUBufferHolderMap;
class PlaceholderData;
class RenderProxyList;
class RenderView;
class View;
class DrawCallCollection;

HYP_API extern uint32 GetRenderThreadFrameIndex();
HYP_API extern uint32 GetGameThreadFrameIndex();

HYP_API extern void BeginFrame_GameThread();
HYP_API extern void EndFrame_GameThread();

HYP_API extern void BeginFrame_RenderThread();
HYP_API extern void EndFrame_RenderThread();

// Acquire a draw collection for the game thread to write to for a certain view
HYP_API extern RenderProxyList& GetProducerRenderProxyList(View* view);
HYP_API extern RenderProxyList& GetConsumerRenderProxyList(View* view);

HYP_API extern DrawCallCollection* AllocateDrawCallCollection(View* view);

class RenderGlobalState
{
public:
    enum IndexAllocatorType
    {
        ENV_PROBE = 0,
        MAX
    };

    static constexpr uint32 index_allocator_maximums[IndexAllocatorType::MAX] = {
        16 // ENV_PROBE
    };

    struct IndexAllocator
    {
        static constexpr uint32 invalid_index = ~0u;

        uint32 current_index = invalid_index;
        Array<uint32> free_indices;

        uint32 AllocateIndex(uint32 max)
        {
            if (free_indices.Empty())
            {
                if (current_index + 1 >= max)
                {
                    return invalid_index;
                }

                current_index++;

                return current_index;
            }
            else
            {
                const uint32 index = free_indices.PopBack();
                return index;
            }
        }

        void FreeIndex(uint32 index)
        {
            free_indices.PushBack(index);
        }
    };

    RenderGlobalState();
    RenderGlobalState(const RenderGlobalState& other) = delete;
    RenderGlobalState& operator=(const RenderGlobalState& other) = delete;
    ~RenderGlobalState();

    void Create();
    void Destroy();
    void UpdateBuffers(FrameBase* frame);

    uint32 AllocateIndex(IndexAllocatorType type);
    void FreeIndex(IndexAllocatorType type, uint32 index);

    GPUBufferHolderBase* Worlds;
    GPUBufferHolderBase* Cameras;
    GPUBufferHolderBase* Lights;
    GPUBufferHolderBase* Entities;
    GPUBufferHolderBase* Materials;
    GPUBufferHolderBase* Skeletons;
    GPUBufferHolderBase* ShadowMaps;
    GPUBufferHolderBase* EnvProbes;
    GPUBufferHolderBase* EnvGrids;
    GPUBufferHolderBase* LightmapVolumes;

    BindlessStorage BindlessTextures;

    UniquePtr<class ShadowMapAllocator> ShadowMapAllocator;
    UniquePtr<class GPUBufferHolderMap> GPUBufferHolderMap;
    UniquePtr<PlaceholderData> PlaceholderData;

    DescriptorTableRef GlobalDescriptorTable;

private:
    void CreateBlueNoiseBuffer();
    void CreateSphereSamplesBuffer();

    void SetDefaultDescriptorSetElements(uint32 frame_index);

    IndexAllocator m_index_allocators[IndexAllocatorType::MAX];
};

} // namespace hyperion

#endif
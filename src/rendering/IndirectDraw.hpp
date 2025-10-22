/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FixedArray.hpp>
#include <core/containers/Array.hpp>

#include <rendering/CullData.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderMemory.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

namespace hyperion {

class Mesh;
class Material;
class Entity;
struct RenderSetup;

struct RenderCommand_CreateIndirectRenderer;
struct RenderCommand_DestroyIndirectRenderer;

struct DrawCallStorage;
struct InstancedDrawCallStorage;

class DrawCallCollection;
class IDrawCallCollectionImpl;

struct alignas(16) ObjectInstance
{
    uint32 entityId;
    uint32 drawCommandIndex;
    uint32 batchIndex;
};

static_assert(sizeof(ObjectInstance) == 16);

struct DrawCommandData
{
    uint32 drawCommandIndex;
};

class IndirectDrawState
{
public:
    static constexpr uint32 BatchSize = 256;
    static constexpr uint32 InitialCount = BatchSize;
    // should sizes be scaled up to the next power of 2?
    static constexpr bool UseNextPow2Size = true;

    IndirectDrawState();
    ~IndirectDrawState();

    HYP_FORCE_INLINE const GpuBufferRef& GetInstanceBuffer(uint32 frameIndex) const
    {
        return m_instanceBuffers[frameIndex];
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetIndirectBuffer(uint32 frameIndex) const
    {
        return m_indirectBuffers[frameIndex];
    }

    HYP_FORCE_INLINE const Array<ObjectInstance, RenderAllocator>& GetInstances() const
    {
        return m_objectInstances;
    }

    void Create();

    void PushDrawCall(SizeType drawCallIndex, const DrawCallStorage& drawCalls, DrawCommandData& out);
    void PushInstancedDrawCall(SizeType drawCallIndex, const InstancedDrawCallStorage& drawCalls, DrawCommandData& out);

    void ResetDrawState();

    void UpdateBufferData(FrameBase* frame, bool* outWasResized);

private:
    Array<ObjectInstance, RenderAllocator> m_objectInstances;
    TByteBuffer<RenderAllocator> m_drawCommandsBuffer;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_indirectBuffers;
    FixedArray<GpuBufferRef, NumFramesInFlight> m_instanceBuffers;
    FixedArray<GpuBufferRef, NumFramesInFlight> m_stagingBuffers;
    uint32 m_numDrawCommands;
    uint8 m_dirtyBits;
};

class IndirectRenderer
{
public:
    friend struct RenderCommand_CreateIndirectRenderer;
    friend struct RenderCommand_DestroyIndirectRenderer;

    IndirectRenderer();
    IndirectRenderer(const IndirectRenderer&) = delete;
    IndirectRenderer& operator=(const IndirectRenderer&) = delete;
    IndirectRenderer(IndirectRenderer&&) noexcept = delete;
    IndirectRenderer& operator=(IndirectRenderer&&) noexcept = delete;
    ~IndirectRenderer();

    HYP_FORCE_INLINE IndirectDrawState& GetDrawState()
    {
        return m_indirectDrawState;
    }

    HYP_FORCE_INLINE const IndirectDrawState& GetDrawState() const
    {
        return m_indirectDrawState;
    }

    void Create(IDrawCallCollectionImpl* impl);

    /*! \brief Register all current draw calls in the draw call collection with the indirect draw state */
    void PushDrawCallsToIndirectState(DrawCallCollection& drawCallCollection);

    void ExecuteCullShaderInBatches(FrameBase* frame, const RenderSetup& renderSetup);

private:
    void RebuildDescriptors(FrameBase* frame);

    IndirectDrawState m_indirectDrawState;
    ComputePipelineRef m_objectVisibility;
    CullData m_cachedCullData;
    uint8 m_cachedCullDataUpdatedBits;
    IDrawCallCollectionImpl* m_drawCallCollectionImpl;
};

} // namespace hyperion

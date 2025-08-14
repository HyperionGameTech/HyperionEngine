/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FixedArray.hpp>
#include <core/containers/Array.hpp>

#include <rendering/CullData.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

namespace hyperion {

class Mesh;
class Material;
class Entity;
struct RenderSetup;

struct RenderCommand_CreateIndirectRenderer;
struct RenderCommand_DestroyIndirectRenderer;

struct DrawCall;
struct InstancedDrawCall;

class DrawCallCollection;
class IDrawCallCollectionImpl;

struct alignas(16) ObjectInstance
{
    uint32 meshEntityBinding;
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
    static constexpr uint32 batchSize = 256u;
    static constexpr uint32 initialCount = batchSize;
    // should sizes be scaled up to the next power of 2?
    static constexpr bool useNextPow2Size = true;

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

    HYP_FORCE_INLINE const Array<ObjectInstance>& GetInstances() const
    {
        return m_objectInstances;
    }

    void Create();

    void PushDrawCall(const DrawCall& drawCall, DrawCommandData& out);
    void PushInstancedDrawCall(const InstancedDrawCall& drawCall, DrawCommandData& out);

    void ResetDrawState();

    void UpdateBufferData(FrameBase* frame, bool* outWasResized);

private:
    Array<ObjectInstance> m_objectInstances;
    ByteBuffer m_drawCommandsBuffer;

    FixedArray<GpuBufferRef, g_framesInFlight> m_indirectBuffers;
    FixedArray<GpuBufferRef, g_framesInFlight> m_instanceBuffers;
    FixedArray<GpuBufferRef, g_framesInFlight> m_stagingBuffers;
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

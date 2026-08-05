/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/Array.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderMemory.hpp>

#include <Core/Math/Mat4f.hpp>

namespace Hyperion {

class Mesh;
class Entity;
struct RenderSetup;

struct DrawCallStorage;
struct InstancedDrawCallStorage;

class DrawCallCollection;
class EntityBatchAllocatorBase;

struct ObjectInstance
{
    Mat4f transform;
    uint32 entityBindingIndex;
    uint32 drawCommandIndex;
    uint32 batchIndex;
    uint32 instanceIndex; // index of data in the batch
};

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

    HYP_FORCE_INLINE const Array<ObjectInstance, RHIAllocator>& GetInstances() const
    {
        return m_objectInstances;
    }

    void Create();

    void PushDrawCall(size_t drawCallIndex, const DrawCallStorage& drawCalls, DrawCommandData& out);
    void PushInstancedDrawCall(size_t drawCallIndex, const InstancedDrawCallStorage& drawCalls, DrawCommandData& out);

    void UpdateBufferData(CommandRecorder& cr, bool* outWasResized);

    void ResetDrawState();

private:
    Array<ObjectInstance, RHIAllocator> m_objectInstances;
    Array<IndirectDrawCommand, RHIAllocator> m_drawCommandsBuffer;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_indirectBuffers;
    FixedArray<GpuBufferRef, NumFramesInFlight> m_instanceBuffers;

    uint32 m_numDrawCommands;
    uint8 m_dirtyBits;
};

class IndirectRenderer
{
public:
    friend struct CreateIndirectRenderer;
    friend struct DestroyIndirectRenderer;

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

    void Create(EntityBatchAllocatorBase* batchAllocator);

    /*! \brief Register all current draw calls in the draw call collection with the indirect draw state */
    void PushDrawCallsToIndirectState(CommandRecorder& cr, DrawCallCollection& drawCallCollection);
    void ExecuteCullShaderInBatches(CommandRecorder& cr, const RenderSetup& renderSetup);

private:
    void PrepareDrawCommands(CommandRecorder& cr);

    IndirectDrawState m_indirectDrawState;
    EntityBatchAllocatorBase* m_batchAllocator;
};

} // namespace Hyperion

#ifndef HYPERION_V2_INDIRECT_DRAW_H
#define HYPERION_V2_INDIRECT_DRAW_H

#include <core/Base.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/Compute.hpp>
#include <rendering/CullData.hpp>

#include <core/lib/Queue.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/UniquePtr.hpp>

#include <math/BoundingSphere.hpp>

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::IndirectDrawCommand;
using renderer::IndirectBuffer;
using renderer::StorageBuffer;
using renderer::StagingBuffer;
using renderer::Frame;
using renderer::Result;

class Mesh;
class Material;
class Engine;
class Entity;

struct RenderCommand_CreateIndirectRenderer;
struct RenderCommand_DestroyIndirectRenderer;

class IndirectDrawState
{
public:
    static constexpr UInt batch_size = 256u;
    static constexpr UInt initial_count = batch_size;
    // should sizes be scaled up to the next power of 2?
    static constexpr bool use_next_pow2_size = true;

    IndirectDrawState();
    ~IndirectDrawState();

    StorageBuffer *GetInstanceBuffer(UInt frame_index) const
        { return m_instance_buffers[frame_index].Get(); }

    IndirectBuffer *GetIndirectBuffer(UInt frame_index) const
        { return m_indirect_buffers[frame_index].Get(); }

    Array<EntityDrawProxy> &GetDrawProxies()
        { return m_draw_proxies; }

    const Array<EntityDrawProxy> &GetDrawProxies() const
        { return m_draw_proxies; }

    Result Create(Engine *engine);
    Result Destroy(Engine *engine);

    void PushDrawProxy(const EntityDrawProxy &draw_proxy);
    void Reset();
    void Reserve(Engine *engine, Frame *frame, SizeType count);

    void UpdateBufferData(Engine *engine, Frame *frame, bool *out_was_resized);

private:
    bool ResizeIndirectDrawCommandsBuffer(Engine *engine, Frame *frame, SizeType count);
    bool ResizeInstancesBuffer(Engine *engine, Frame *frame, SizeType count);

    // returns true if resize happened.
    bool ResizeIfNeeded(Engine *engine, Frame *frame, SizeType count);

    Array<ObjectInstance> m_object_instances;
    Array<EntityDrawProxy> m_draw_proxies;

    FixedArray<UniquePtr<IndirectBuffer>, max_frames_in_flight> m_indirect_buffers;
    FixedArray<UniquePtr<StorageBuffer>, max_frames_in_flight> m_instance_buffers;
    FixedArray<UniquePtr<StagingBuffer>, max_frames_in_flight> m_staging_buffers;
    FixedArray<bool, max_frames_in_flight> m_is_dirty;
    UInt m_max_entity_id = 0;
};

struct alignas(16) IndirectParams
{
};

class IndirectRenderer
{
public:
    friend struct RenderCommand_CreateIndirectRenderer;
    friend struct RenderCommand_DestroyIndirectRenderer;

    IndirectRenderer();
    ~IndirectRenderer();

    IndirectDrawState &GetDrawState()
        { return m_indirect_draw_state; }

    const IndirectDrawState &GetDrawState() const
        { return m_indirect_draw_state; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void ExecuteCullShaderInBatches(
        Engine *engine,
        Frame *frame,
        const CullData &cull_data
    );

private:
    void RebuildDescriptors(Engine *engine, Frame *frame);

    IndirectDrawState m_indirect_draw_state;
    Handle<ComputePipeline> m_object_visibility;
    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;
    FixedArray<UniformBuffer, max_frames_in_flight> m_indirect_params_buffers;
    CullData m_cached_cull_data;
    UInt8 m_cached_cull_data_updated_bits;
};

} // namespace hyperion::v2

#endif
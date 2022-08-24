#ifndef HYPERION_V2_INDIRECT_DRAW_H
#define HYPERION_V2_INDIRECT_DRAW_H

#include "Base.hpp"
#include "DrawProxy.hpp"
#include "Compute.hpp"
#include "CullData.hpp"

#include <core/lib/Queue.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/DynArray.hpp>

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

class IndirectDrawState
{
public:
    static constexpr UInt batch_size = 256u;
    static constexpr UInt initial_count = batch_size;
    // should sizes be scaled up to the next power of 2
    static constexpr bool use_next_pow2_size = true;

    IndirectDrawState();
    ~IndirectDrawState();

    StorageBuffer *GetInstanceBuffer(UInt frame_index) const
        { return m_instance_buffers[frame_index].get(); }

    IndirectBuffer *GetIndirectBuffer(UInt frame_index) const
        { return m_indirect_buffers[frame_index].get(); }

    DynArray<EntityDrawProxy> &GetDrawProxies()             { return m_draw_proxies; }
    const DynArray<EntityDrawProxy> &GetDrawProxies() const { return m_draw_proxies; }

    Result Create(Engine *engine);
    Result Destroy(Engine *engine);

    void PushDrawProxy(const EntityDrawProxy &draw_proxy);
    void ResetDrawProxies();
    void Reserve(Engine *engine, Frame *frame, SizeType count);

    void UpdateBufferData(Engine *engine, Frame *frame, bool *out_was_resized);

private:
    bool ResizeIndirectDrawCommandsBuffer(Engine *engine, Frame *frame, SizeType count);
    bool ResizeInstancesBuffer(Engine *engine, Frame *frame, SizeType count);

    // returns true if resize happened.
    bool ResizeIfNeeded(Engine *engine, Frame *frame, SizeType count);

    DynArray<ObjectInstance>      m_object_instances;
    DynArray<EntityDrawProxy>     m_draw_proxies;

    FixedArray<std::unique_ptr<IndirectBuffer>, max_frames_in_flight> m_indirect_buffers;
    FixedArray<std::unique_ptr<StorageBuffer>, max_frames_in_flight>  m_instance_buffers;
    FixedArray<std::unique_ptr<StagingBuffer>, max_frames_in_flight>  m_staging_buffers;
    FixedArray<bool, max_frames_in_flight>                            m_is_dirty;
    UInt m_max_entity_id = 0;

};

struct alignas(16) IndirectParams {
};

class IndirectRenderer {
public:
    IndirectRenderer();
    ~IndirectRenderer();

    IndirectDrawState &GetDrawState()             { return m_indirect_draw_state; }
    const IndirectDrawState &GetDrawState() const { return m_indirect_draw_state; }

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
    FixedArray<std::unique_ptr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;
    FixedArray<UniformBuffer, max_frames_in_flight> m_indirect_params_buffers;
    CullData m_cached_cull_data;
    UInt8 m_cached_cull_data_updated_bits;
};

} // namespace hyperion::v2

#endif
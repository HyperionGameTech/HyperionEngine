#ifndef HYPERION_V2_INDIRECT_DRAW_H
#define HYPERION_V2_INDIRECT_DRAW_H

#include <core/Base.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/Compute.hpp>
#include <rendering/CullData.hpp>
// #include <rendering/DrawCall.hpp>

#include <core/lib/Queue.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/ID.hpp>

#include <math/BoundingSphere.hpp>

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::IndirectDrawCommand;
using renderer::Frame;
using renderer::Result;

class Mesh;
class Material;
class Engine;
class Entity;

struct RenderCommand_CreateIndirectRenderer;
struct RenderCommand_DestroyIndirectRenderer;

struct DrawCall;

struct DrawCommandData
{
    UInt draw_command_index;
};

class IndirectDrawState
{
public:
    static constexpr UInt batch_size = 256u;
    static constexpr UInt initial_count = batch_size;
    // should sizes be scaled up to the next power of 2?
    static constexpr bool use_next_pow2_size = true;

    IndirectDrawState();
    ~IndirectDrawState();

    const GPUBufferRef &GetInstanceBuffer(UInt frame_index) const
        { return m_instance_buffers[frame_index]; }

    const GPUBufferRef &GetIndirectBuffer(UInt frame_index) const
        { return m_indirect_buffers[frame_index]; }

    const Array<ObjectInstance> &GetInstances() const
        { return m_object_instances; }

    Result Create();
    void Destroy();

    void PushDrawCall(const DrawCall &draw_call, DrawCommandData &out);
    void Reset();
    void Reserve(Frame *frame, SizeType count);

    void UpdateBufferData(Frame *frame, bool *out_was_resized);

private:
    bool ResizeIndirectDrawCommandsBuffer(Frame *frame, SizeType count);
    bool ResizeInstancesBuffer(Frame *frame, SizeType count);

    // returns true if resize happened.
    bool ResizeIfNeeded(Frame *frame);

    Array<ObjectInstance>                           m_object_instances;
    Array<IndirectDrawCommand>                      m_draw_commands;

    FixedArray<GPUBufferRef, max_frames_in_flight>  m_indirect_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>  m_instance_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>  m_staging_buffers;
    FixedArray<bool, max_frames_in_flight>          m_is_dirty;
    UInt32                                          m_num_draw_commands;
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

    void Create();
    void Destroy();

    void ExecuteCullShaderInBatches(Frame *frame, const CullData &cull_data);

private:
    void RebuildDescriptors(Frame *frame);

    IndirectDrawState m_indirect_draw_state;
    Handle<ComputePipeline> m_object_visibility;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_descriptor_sets;
    CullData m_cached_cull_data;
    UInt8 m_cached_cull_data_updated_bits;
};

} // namespace hyperion::v2

#endif
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_INDIRECT_DRAW_HPP
#define HYPERION_INDIRECT_DRAW_HPP

#include <core/containers/FixedArray.hpp>
#include <core/containers/Array.hpp>

#include <rendering/CullData.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {

using renderer::IndirectDrawCommand;

class Mesh;
class Material;
class Engine;
class Entity;

struct RenderCommand_CreateIndirectRenderer;
struct RenderCommand_DestroyIndirectRenderer;

struct DrawCall;
class DrawCallCollection;

struct alignas(16) ObjectInstance
{
    uint32  entity_id;
    uint32  draw_command_index;
    uint32  instance_index;
    uint32  batch_index;
};

static_assert(sizeof(ObjectInstance) == 16);

struct DrawCommandData
{
    uint32  draw_command_index;
};

class IndirectDrawState
{
public:
    static constexpr uint32 batch_size = 256u;
    static constexpr uint32 initial_count = batch_size;
    // should sizes be scaled up to the next power of 2?
    static constexpr bool use_next_pow2_size = true;

    IndirectDrawState();
    ~IndirectDrawState();

    HYP_FORCE_INLINE const GPUBufferRef &GetInstanceBuffer(uint32 frame_index) const
        { return m_instance_buffers[frame_index]; }

    HYP_FORCE_INLINE const GPUBufferRef &GetIndirectBuffer(uint32 frame_index) const
        { return m_indirect_buffers[frame_index]; }

    HYP_FORCE_INLINE const Array<ObjectInstance> &GetInstances() const
        { return m_object_instances; }

    void Create();
    void Destroy();

    void PushDrawCall(const DrawCall &draw_call, DrawCommandData &out);
    void ResetDrawState();

    void UpdateBufferData(FrameBase *frame, bool *out_was_resized);

private:
    Array<ObjectInstance>                           m_object_instances;
    Array<IndirectDrawCommand>                      m_draw_commands;

    FixedArray<GPUBufferRef, max_frames_in_flight>  m_indirect_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>  m_instance_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>  m_staging_buffers;
    uint32                                          m_num_draw_commands;
    uint8                                           m_dirty_bits;
};

class IndirectRenderer
{
public:
    friend struct RenderCommand_CreateIndirectRenderer;
    friend struct RenderCommand_DestroyIndirectRenderer;

    IndirectRenderer(DrawCallCollection *draw_call_collection);
    IndirectRenderer(const IndirectRenderer &)                  = delete;
    IndirectRenderer &operator=(const IndirectRenderer &)       = delete;
    IndirectRenderer(IndirectRenderer &&) noexcept              = delete;
    IndirectRenderer &operator=(IndirectRenderer &&) noexcept   = delete;
    ~IndirectRenderer();

    HYP_FORCE_INLINE IndirectDrawState &GetDrawState()
        { return m_indirect_draw_state; }

    HYP_FORCE_INLINE const IndirectDrawState &GetDrawState() const
        { return m_indirect_draw_state; }

    void Create();
    void Destroy();

    /*! \brief Register all current draw calls in the draw call collection with the indirect draw state */
    void PushDrawCallsToIndirectState();

    void ExecuteCullShaderInBatches(FrameBase *frame, const CullData &cull_data);

private:
    void RebuildDescriptors(FrameBase *frame);

    DrawCallCollection                                  *m_draw_call_collection;
    IndirectDrawState                                   m_indirect_draw_state;
    ComputePipelineRef                                  m_object_visibility;
    CullData                                            m_cached_cull_data;
    uint8                                               m_cached_cull_data_updated_bits;
};

} // namespace hyperion

#endif
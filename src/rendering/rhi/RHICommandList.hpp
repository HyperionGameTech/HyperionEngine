/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RHI_COMMAND_LIST_HPP
#define HYPERION_RHI_COMMAND_LIST_HPP

#include <core/memory/MemoryPool.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class RHICommandBase;

class RHICommandMemoryPoolBase
{
public:
    virtual ~RHICommandMemoryPoolBase() = default;

    virtual void FreeCommand(RHICommandBase *command) = 0;
};

struct RHICommandPoolHandle
{
    RHICommandMemoryPoolBase    *pool;
    uint32                      index;
};

class RHICommandBase
{
public:
    friend class RHICommandList;

    template <class T>
    friend class RHICommandMemoryPool;

    RHICommandBase()                                                = default;
    RHICommandBase(const RHICommandBase &other)                     = delete;
    RHICommandBase &operator=(const RHICommandBase &other)          = delete;
    RHICommandBase(RHICommandBase &&other) noexcept                 = delete;
    RHICommandBase &operator=(RHICommandBase &&other) noexcept      = delete;
    virtual ~RHICommandBase()                                       = default;

    virtual void Execute(const CommandBufferRef &cmd) = 0;

private:
    RHICommandPoolHandle    m_pool_handle;
};

template <class RHICommandType>
class RHICommandMemoryPool final : public MemoryPool<ValueStorage<RHICommandType>>, RHICommandMemoryPoolBase
{
public:
    static RHICommandMemoryPool<RHICommandType> &GetInstance()
    {
        static RHICommandMemoryPool<RHICommandType> instance;

        return instance;
    }

    virtual ~RHICommandMemoryPool() override = default;

    template <class... Args>
    RHICommandType *NewCommand(Args &&... args)
    {
        ValueStorage<RHICommandType> *command_storage;

        const uint32 index = MemoryPool<ValueStorage<RHICommandType>>::AcquireIndex(&command_storage);

        command_storage->Construct(std::forward<Args>(args)...);
        command_storage->Get().m_pool_handle = { static_cast<RHICommandMemoryPoolBase *>(this), index };

        return &command_storage->Get();
    }

    virtual void FreeCommand(RHICommandBase *command) override
    {
        AssertDebug(command != nullptr);
        AssertDebug(command->m_pool_handle.pool == this);

        command->~RHICommandBase();

        MemoryPool<ValueStorage<RHICommandType>>::ReleaseIndex(command->m_pool_handle.index);
    }
};

class BindVertexBuffer final : public RHICommandBase
{
public:
    BindVertexBuffer(const GPUBufferRef &buffer)
        : m_buffer(buffer)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        cmd->BindVertexBuffer(m_buffer);
    }

private:
    GPUBufferRef    m_buffer;
};

class BindIndexBuffer final : public RHICommandBase
{
public:
    BindIndexBuffer(const GPUBufferRef &buffer)
        : m_buffer(buffer)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        cmd->BindIndexBuffer(m_buffer);
    }

private:
    GPUBufferRef    m_buffer;
};

class DrawIndexed final : public RHICommandBase
{
public:
    DrawIndexed(uint32 num_indices, uint32 num_instance, uint32 instance_index)
        : m_num_indices(num_indices),
          m_num_instances(num_instance),
          m_instance_index(instance_index)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        cmd->DrawIndexed(m_num_indices, m_num_instances, m_instance_index);
    }

private:
    uint32  m_num_indices;
    uint32  m_num_instances;
    uint32  m_instance_index;
};

class DrawIndexedIndirect final : public RHICommandBase
{
public:
    DrawIndexedIndirect(const GPUBufferRef &buffer, uint32 buffer_offset)
        : m_buffer(buffer),
          m_buffer_offset(buffer_offset)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        cmd->DrawIndexedIndirect(m_buffer, m_buffer_offset);
    }

private:
    GPUBufferRef    m_buffer;
    uint32          m_buffer_offset;
};

class BeginFramebuffer final : public RHICommandBase
{
public:
    BeginFramebuffer(const FramebufferRef &framebuffer, uint32 frame_index)
        : m_framebuffer(framebuffer),
          m_frame_index(frame_index)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_framebuffer->BeginCapture(cmd, m_frame_index);
    }

private:
    FramebufferRef  m_framebuffer;
    uint32          m_frame_index;
};

class EndFramebuffer final : public RHICommandBase
{
public:
    EndFramebuffer(const FramebufferRef &framebuffer, uint32 frame_index)
        : m_framebuffer(framebuffer),
          m_frame_index(frame_index)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_framebuffer->EndCapture(cmd, m_frame_index);
    }

private:
    FramebufferRef  m_framebuffer;
    uint32          m_frame_index;
};

class BindGraphicsPipeline final : public RHICommandBase
{
public:
    BindGraphicsPipeline(const GraphicsPipelineRef &pipeline, Vec2i viewport_offset, Vec2i viewport_extent)
        : m_pipeline(pipeline),
          m_viewport_offset(viewport_offset),
          m_viewport_extent(viewport_extent)
    {
    }

    BindGraphicsPipeline(const GraphicsPipelineRef &pipeline)
        : m_pipeline(pipeline)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        if (m_viewport_offset != Vec2i(0, 0) || m_viewport_extent != Vec2i(0, 0)) {
            m_pipeline->Bind(cmd, m_viewport_offset, m_viewport_extent);
        } else {
            m_pipeline->Bind(cmd);
        }
    }

private:
    GraphicsPipelineRef m_pipeline;
    Vec2i               m_viewport_offset;
    Vec2i               m_viewport_extent;
};

class BindComputePipeline final : public RHICommandBase
{
public:
    BindComputePipeline(const ComputePipelineRef &pipeline)
        : m_pipeline(pipeline)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_pipeline->Bind(cmd);
    }

private:
    ComputePipelineRef m_pipeline;
};

class BindRaytracingPipeline final : public RHICommandBase
{
public:
    BindRaytracingPipeline(const RaytracingPipelineRef &pipeline)
        : m_pipeline(pipeline)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_pipeline->Bind(cmd);
    }

private:
    RaytracingPipelineRef   m_pipeline;
};

class BindDescriptorSet final : public RHICommandBase
{
public:
    BindDescriptorSet(const DescriptorSetRef &descriptor_set, const GraphicsPipelineRef &graphics_pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index)
        : m_descriptor_set(descriptor_set),
          m_graphics_pipeline(graphics_pipeline),
          m_offsets(offsets),
          m_bind_index(bind_index)
    {
    }

    BindDescriptorSet(const DescriptorSetRef &descriptor_set, const ComputePipelineRef &compute_pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index)
        : m_descriptor_set(descriptor_set),
          m_compute_pipeline(compute_pipeline),
          m_offsets(offsets),
          m_bind_index(bind_index)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        if (m_graphics_pipeline) {
            m_descriptor_set->Bind(cmd, m_graphics_pipeline, m_offsets, m_bind_index);
        } else if (m_compute_pipeline) {
            m_descriptor_set->Bind(cmd, m_compute_pipeline, m_offsets, m_bind_index);
        } else {
            AssertThrowMsg(false, "No pipeline bound to descriptor set");
        }
    }

private:
    DescriptorSetRef        m_descriptor_set;
    GraphicsPipelineRef     m_graphics_pipeline;
    ComputePipelineRef      m_compute_pipeline;
    ArrayMap<Name, uint32>  m_offsets;
    uint32                  m_bind_index;
};


class BindDescriptorTable final : public RHICommandBase
{
public:
    BindDescriptorTable(const DescriptorTableRef &descriptor_table, const GraphicsPipelineRef &graphics_pipeline, const ArrayMap<Name, ArrayMap<Name, uint32>> &offsets, uint32 frame_index)
        : m_descriptor_table(descriptor_table),
          m_graphics_pipeline(graphics_pipeline),
          m_offsets(offsets),
          m_frame_index(frame_index)
    {
    }

    BindDescriptorTable(const DescriptorTableRef &descriptor_table, const ComputePipelineRef &compute_pipeline, const ArrayMap<Name, ArrayMap<Name, uint32>> &offsets, uint32 frame_index)
        : m_descriptor_table(descriptor_table),
          m_compute_pipeline(compute_pipeline),
          m_offsets(offsets),
          m_frame_index(frame_index)
    {
    }

    BindDescriptorTable(const DescriptorTableRef &descriptor_table, const RaytracingPipelineRef &raytracing_pipeline, const ArrayMap<Name, ArrayMap<Name, uint32>> &offsets, uint32 frame_index)
        : m_descriptor_table(descriptor_table),
          m_raytracing_pipeline(raytracing_pipeline),
          m_offsets(offsets),
          m_frame_index(frame_index)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        if (m_graphics_pipeline) {
            m_descriptor_table->Bind(cmd, m_frame_index, m_graphics_pipeline, m_offsets);
        } else if (m_compute_pipeline) {
            m_descriptor_table->Bind(cmd, m_frame_index, m_compute_pipeline, m_offsets);
        } else if (m_raytracing_pipeline) {
            m_descriptor_table->Bind(cmd, m_frame_index, m_raytracing_pipeline, m_offsets);
        } else {
            HYP_UNREACHABLE();
        }
    }

private:
    DescriptorTableRef                      m_descriptor_table;
    GraphicsPipelineRef                     m_graphics_pipeline;
    ComputePipelineRef                      m_compute_pipeline;
    RaytracingPipelineRef                   m_raytracing_pipeline;
    ArrayMap<Name, ArrayMap<Name, uint32>>  m_offsets;
    uint32                                  m_frame_index;
};

class InsertBarrier final : public RHICommandBase
{
public:
    InsertBarrier(const GPUBufferRef &buffer, const renderer::ResourceState &state, renderer::ShaderModuleType shader_module_type = renderer::ShaderModuleType::UNSET)
        : m_buffer(buffer),
          m_state(state),
          m_shader_module_type(shader_module_type)
    {
    }

    InsertBarrier(const ImageRef &image, const renderer::ResourceState &state, renderer::ShaderModuleType shader_module_type = renderer::ShaderModuleType::UNSET)
        : m_image(image),
          m_state(state),
          m_shader_module_type(shader_module_type)
    {
    }

    InsertBarrier(const ImageRef &image, const renderer::ResourceState &state, const renderer::ImageSubResource &sub_resource, renderer::ShaderModuleType shader_module_type = renderer::ShaderModuleType::UNSET)
        : m_image(image),
          m_state(state),
          m_sub_resource(sub_resource),
          m_shader_module_type(shader_module_type)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        if (m_buffer) {
            m_buffer->InsertBarrier(cmd, m_state, m_shader_module_type);
        } else if (m_image) {
            if (m_sub_resource) {
                m_image->InsertSubResourceBarrier(cmd, *m_sub_resource, m_state, m_shader_module_type);
            } else {
                m_image->InsertBarrier(cmd, m_state, m_shader_module_type);
            }
        }
    }

private:
    GPUBufferRef                            m_buffer;
    ImageRef                                m_image;
    renderer::ResourceState                 m_state;
    Optional<renderer::ImageSubResource>    m_sub_resource;
    renderer::ShaderModuleType              m_shader_module_type;
};

class Blit final : public RHICommandBase
{
public:
    Blit(const ImageRef &src_image, const ImageRef &dst_image)
        : m_src_image(src_image),
          m_dst_image(dst_image)
    {
    }

    Blit(const ImageRef &src_image, const ImageRef &dst_image, const Rect<uint32> &src_rect, const Rect<uint32> &dst_rect)
        : m_src_image(src_image),
          m_dst_image(dst_image),
          m_src_rect(src_rect),
          m_dst_rect(dst_rect)
    {
    }

    Blit(const ImageRef &src_image, const ImageRef &dst_image, const Rect<uint32> &src_rect, const Rect<uint32> &dst_rect, uint32 src_mip, uint32 dst_mip, uint32 src_face, uint32 dst_face)
        : m_src_image(src_image),
          m_dst_image(dst_image),
          m_src_rect(src_rect),
          m_dst_rect(dst_rect),
          m_mip_face_info(MipFaceInfo { src_mip, dst_mip, src_face, dst_face })
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        if (m_mip_face_info) {
            MipFaceInfo info = *m_mip_face_info;

            if (m_src_rect && m_dst_rect) {
                m_dst_image->Blit(cmd, m_src_image, *m_src_rect, *m_dst_rect, info.src_mip, info.dst_mip, info.src_face, info.dst_face);
            } else {
                m_dst_image->Blit(cmd, m_src_image, info.src_mip, info.dst_mip, info.src_face, info.dst_face);
            }
        } else {
            if (m_src_rect && m_dst_rect) {
                m_dst_image->Blit(cmd, m_src_image, *m_src_rect, *m_dst_rect);
            } else {
                m_dst_image->Blit(cmd, m_src_image);
            }
        }
    }

private:
    struct MipFaceInfo
    {
        uint32  src_mip;
        uint32  dst_mip;
        uint32  src_face;
        uint32  dst_face;
    };

    ImageRef                m_src_image;
    ImageRef                m_dst_image;

    Optional<Rect<uint32>>  m_src_rect;
    Optional<Rect<uint32>>  m_dst_rect;

    Optional<MipFaceInfo>   m_mip_face_info;
};

class BlitRect final : public RHICommandBase
{
public:
    BlitRect(const ImageRef &src_image, const ImageRef &dst_image, const Rect<uint32> &src_rect, const Rect<uint32> &dst_rect)
        : m_src_image(src_image),
          m_dst_image(dst_image),
          m_src_rect(src_rect),
          m_dst_rect(dst_rect)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_dst_image->Blit(cmd, m_src_image, m_src_rect, m_dst_rect);
    }

private:
    ImageRef        m_src_image;
    ImageRef        m_dst_image;
    Rect<uint32>    m_src_rect;
    Rect<uint32>    m_dst_rect;
};

class CopyImageToBuffer final : public RHICommandBase
{
public:
    CopyImageToBuffer(const ImageRef &image, const GPUBufferRef &buffer)
        : m_image(image),
          m_buffer(buffer)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_image->CopyToBuffer(cmd, m_buffer);
    }

private:
    ImageRef        m_image;
    GPUBufferRef    m_buffer;
};

class CopyBufferToImage final : public RHICommandBase
{
public:
    CopyBufferToImage(const GPUBufferRef &buffer, const ImageRef &image)
        : m_buffer(buffer),
          m_image(image)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_image->CopyFromBuffer(cmd, m_buffer);
    }

private:
    GPUBufferRef    m_buffer;
    ImageRef        m_image;
};

class CopyBuffer final : public RHICommandBase
{
public:
    CopyBuffer(const GPUBufferRef &src_buffer, const GPUBufferRef &dst_buffer, SizeType size)
        : m_src_buffer(src_buffer),
          m_dst_buffer(dst_buffer),
          m_size(size)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_dst_buffer->CopyFrom(cmd, m_src_buffer, m_size);
    }
private:
    GPUBufferRef    m_src_buffer;
    GPUBufferRef    m_dst_buffer;
    SizeType        m_size;
};

class GenerateMipmaps final : public RHICommandBase
{
public:
    GenerateMipmaps(const ImageRef &image)
        : m_image(image)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_image->GenerateMipmaps(cmd);
    }

private:
    ImageRef    m_image;
};

class DispatchCompute final : public RHICommandBase
{
public:
    DispatchCompute(const ComputePipelineRef &pipeline, Vec3u workgroup_count)
        : m_pipeline(pipeline),
          m_workgroup_count(workgroup_count)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_pipeline->Dispatch(cmd, m_workgroup_count);
    }

private:
    ComputePipelineRef  m_pipeline;
    Vec3u               m_workgroup_count;
};

class TraceRays final : public RHICommandBase
{
public:
    TraceRays(const RaytracingPipelineRef &pipeline, const Vec3u &workgroup_count)
        : m_pipeline(pipeline),
          m_workgroup_count(workgroup_count)
    {
    }

    virtual void Execute(const CommandBufferRef &cmd) override
    {
        m_pipeline->TraceRays(cmd, m_workgroup_count);
    }

private:
    RaytracingPipelineRef   m_pipeline;
    Vec3u                   m_workgroup_count;
};

class RHICommandList
{
public:
    RHICommandList();
    RHICommandList(const RHICommandList &other)                   = delete;
    RHICommandList &operator=(const RHICommandList &other)        = delete;
    RHICommandList(RHICommandList &&other) noexcept               = delete;
    RHICommandList &operator=(RHICommandList &&other) noexcept    = delete;
    ~RHICommandList();

    template <class RHICommandType, class... Args>
    void Add(Args &&... args)
    {
        static RHICommandMemoryPool<RHICommandType> &pool = RHICommandMemoryPool<RHICommandType>::GetInstance();

        m_commands.PushBack(pool.NewCommand(std::forward<Args>(args)...));
    }

    void Concat(RHICommandList &&other)
    {
        m_commands.Concat(std::move(other.m_commands));
    }

    void Execute(const CommandBufferRef &cmd);

private:
    void FreeCommand(RHICommandBase *command);

    Array<RHICommandBase *> m_commands;
};

} // namespace hyperion

#endif

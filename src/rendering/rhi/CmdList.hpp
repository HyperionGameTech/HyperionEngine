/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RHI_CMD_LIST_HPP
#define HYPERION_RHI_CMD_LIST_HPP

#include <core/memory/MemoryPool.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Variant.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererGpuBuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RenderObject.hpp>

// #define HYP_RHI_COMMAND_STACK_TRACE

#ifdef HYP_RHI_COMMAND_STACK_TRACE
#include <core/debug/StackDump.hpp>
#endif

namespace hyperion {

class CmdBase;

class CmdMemoryPoolBase
{
public:
    virtual ~CmdMemoryPoolBase() = default;

    virtual void FreeCommand(CmdBase* command) = 0;
};

struct CmdPoolHandle
{
    CmdMemoryPoolBase* pool;
    uint32 index;
};

class CmdBase
{
public:
    friend class CmdList;

    template <class T>
    friend class CmdMemoryPool;

    CmdBase() = default;
    CmdBase(const CmdBase& other) = delete;
    CmdBase& operator=(const CmdBase& other) = delete;
    CmdBase(CmdBase&& other) noexcept = delete;
    CmdBase& operator=(CmdBase&& other) noexcept = delete;
    virtual ~CmdBase() = default;

    virtual void Prepare(FrameBase* frame)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) = 0;

private:
    CmdPoolHandle m_pool_handle;

#ifdef HYP_RHI_COMMAND_STACK_TRACE
    char* m_stack_trace = "";
#endif
};

template <class CmdType>
class CmdMemoryPool final : public MemoryPool<ValueStorage<CmdType>>, CmdMemoryPoolBase
{
public:
    static CmdMemoryPool<CmdType>& GetInstance()
    {
        static CmdMemoryPool<CmdType> instance;

        return instance;
    }

    virtual ~CmdMemoryPool() override = default;

    template <class... Args>
    CmdType* NewCommand(Args&&... args)
    {
        ValueStorage<CmdType>* command_storage;

        const uint32 index = MemoryPool<ValueStorage<CmdType>>::AcquireIndex(&command_storage);

        command_storage->Construct(std::forward<Args>(args)...);
        command_storage->Get().m_pool_handle = { static_cast<CmdMemoryPoolBase*>(this), index };

#ifdef HYP_RHI_COMMAND_STACK_TRACE
        // Allocate string and copy stack trace (creating stack trace is extremely SLOW, enable for debugging only!)
        String dump = StackDump(10, 2).ToString();
        command_storage->Get().m_stack_trace = (char*)std::malloc(dump.Size() + 1);
        std::strncpy(command_storage->Get().m_stack_trace, dump.Data(), dump.Size());
        command_storage->Get().m_stack_trace[dump.Size()] = '\0';
#endif

        return &command_storage->Get();
    }

    virtual void FreeCommand(CmdBase* command) override
    {
        AssertDebug(command != nullptr);
        AssertDebug(command->m_pool_handle.pool == this);

#ifdef HYP_RHI_COMMAND_STACK_TRACE
        if (command->m_stack_trace != nullptr)
        {
            std::free(command->m_stack_trace);
            command->m_stack_trace = nullptr;
        }
#endif

        command->~CmdBase();

        MemoryPool<ValueStorage<CmdType>>::ReleaseIndex(command->m_pool_handle.index);
    }
};

class BindVertexBuffer final : public CmdBase
{
public:
    BindVertexBuffer(const GpuBufferRef& buffer)
        : m_buffer(buffer)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        cmd->BindVertexBuffer(m_buffer);
    }

private:
    GpuBufferRef m_buffer;
};

class BindIndexBuffer final : public CmdBase
{
public:
    BindIndexBuffer(const GpuBufferRef& buffer)
        : m_buffer(buffer)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        cmd->BindIndexBuffer(m_buffer);
    }

private:
    GpuBufferRef m_buffer;
};

class DrawIndexed final : public CmdBase
{
public:
    DrawIndexed(uint32 num_indices, uint32 num_instance, uint32 instance_index)
        : m_num_indices(num_indices),
          m_num_instances(num_instance),
          m_instance_index(instance_index)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        cmd->DrawIndexed(m_num_indices, m_num_instances, m_instance_index);
    }

private:
    uint32 m_num_indices;
    uint32 m_num_instances;
    uint32 m_instance_index;
};

class DrawIndexedIndirect final : public CmdBase
{
public:
    DrawIndexedIndirect(const GpuBufferRef& buffer, uint32 buffer_offset)
        : m_buffer(buffer),
          m_buffer_offset(buffer_offset)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        cmd->DrawIndexedIndirect(m_buffer, m_buffer_offset);
    }

private:
    GpuBufferRef m_buffer;
    uint32 m_buffer_offset;
};

class BeginFramebuffer final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API BeginFramebuffer(const FramebufferRef& framebuffer, uint32 frame_index);
    HYP_API virtual void Prepare(FrameBase* frame) override;
#else
    BeginFramebuffer(const FramebufferRef& framebuffer, uint32 frame_index)
        : m_framebuffer(framebuffer),
          m_frame_index(frame_index)
    {
    }
#endif

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_framebuffer->BeginCapture(cmd, m_frame_index);
    }

private:
    FramebufferRef m_framebuffer;
    uint32 m_frame_index;
};

class EndFramebuffer final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API EndFramebuffer(const FramebufferRef& framebuffer, uint32 frame_index);
    HYP_API virtual void Prepare(FrameBase* frame) override;
#else
    EndFramebuffer(const FramebufferRef& framebuffer, uint32 frame_index)
        : m_framebuffer(framebuffer),
          m_frame_index(frame_index)
    {
    }
#endif

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_framebuffer->EndCapture(cmd, m_frame_index);
    }

private:
    FramebufferRef m_framebuffer;
    uint32 m_frame_index;
};

class BindGraphicsPipeline final : public CmdBase
{
public:
#ifdef HYP_DEBUG_MODE
    HYP_API BindGraphicsPipeline(const GraphicsPipelineRef& pipeline, Vec2i viewport_offset, Vec2u viewport_extent);
    HYP_API BindGraphicsPipeline(const GraphicsPipelineRef& pipeline);
#else
    BindGraphicsPipeline(const GraphicsPipelineRef& pipeline, Vec2i viewport_offset, Vec2u viewport_extent)
        : m_pipeline(pipeline),
          m_viewport_offset(viewport_offset),
          m_viewport_extent(viewport_extent)
    {
    }

    BindGraphicsPipeline(const GraphicsPipelineRef& pipeline)
        : m_pipeline(pipeline)
    {
    }
#endif

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        if (m_viewport_offset != Vec2i(0, 0) || m_viewport_extent != Vec2u(0, 0))
        {
            m_pipeline->Bind(cmd, m_viewport_offset, m_viewport_extent);
        }
        else
        {
            m_pipeline->Bind(cmd);
        }
    }

private:
    GraphicsPipelineRef m_pipeline;
    Vec2i m_viewport_offset;
    Vec2u m_viewport_extent;
};

class BindComputePipeline final : public CmdBase
{
public:
    BindComputePipeline(const ComputePipelineRef& pipeline)
        : m_pipeline(pipeline)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline->Bind(cmd);
    }

private:
    ComputePipelineRef m_pipeline;
};

class BindRaytracingPipeline final : public CmdBase
{
public:
    BindRaytracingPipeline(const RaytracingPipelineRef& pipeline)
        : m_pipeline(pipeline)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline->Bind(cmd);
    }

private:
    RaytracingPipelineRef m_pipeline;
};

class BindDescriptorSet final : public CmdBase
{
public:
    BindDescriptorSet(const DescriptorSetRef& descriptor_set, const GraphicsPipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets = {})
        : m_descriptor_set(descriptor_set),
          m_pipeline(pipeline),
          m_offsets(offsets)
    {
        AssertThrowMsg(descriptor_set != nullptr, "Descriptor set must not be null");
        AssertThrowMsg(descriptor_set->IsCreated(), "Descriptor set is not created yet");

        m_bind_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptor_set->GetLayout().GetName());
        AssertThrowMsg(m_bind_index != ~0u, "Invalid bind index for descriptor set %s", descriptor_set->GetLayout().GetName().LookupString());
    }

    BindDescriptorSet(const DescriptorSetRef& descriptor_set, const GraphicsPipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bind_index)
        : m_descriptor_set(descriptor_set),
          m_pipeline(pipeline),
          m_offsets(offsets),
          m_bind_index(bind_index)
    {
        AssertThrowMsg(descriptor_set != nullptr, "Descriptor set must not be null");
        AssertThrowMsg(descriptor_set->IsCreated(), "Descriptor set is not created yet");
        AssertThrowMsg(m_bind_index != ~0u, "Invalid bind index");
    }

    BindDescriptorSet(const DescriptorSetRef& descriptor_set, const ComputePipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets = {})
        : m_descriptor_set(descriptor_set),
          m_pipeline(pipeline),
          m_offsets(offsets)
    {
        AssertThrowMsg(descriptor_set != nullptr, "Descriptor set must not be null");
        AssertThrowMsg(descriptor_set->IsCreated(), "Descriptor set is not created yet");

        m_bind_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptor_set->GetLayout().GetName());
        AssertThrowMsg(m_bind_index != ~0u, "Invalid bind index for descriptor set %s", descriptor_set->GetLayout().GetName().LookupString());
    }

    BindDescriptorSet(const DescriptorSetRef& descriptor_set, const ComputePipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bind_index)
        : m_descriptor_set(descriptor_set),
          m_pipeline(pipeline),
          m_offsets(offsets),
          m_bind_index(bind_index)
    {
        AssertThrowMsg(descriptor_set != nullptr, "Descriptor set must not be null");
        AssertThrowMsg(descriptor_set->IsCreated(), "Descriptor set is not created yet");
        AssertThrowMsg(m_bind_index != ~0u, "Invalid bind index");
    }

    BindDescriptorSet(const DescriptorSetRef& descriptor_set, const RaytracingPipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets = {})
        : m_descriptor_set(descriptor_set),
          m_pipeline(pipeline),
          m_offsets(offsets)
    {
        AssertThrowMsg(descriptor_set != nullptr, "Descriptor set must not be null");
        AssertThrowMsg(descriptor_set->IsCreated(), "Descriptor set is not created yet");

        m_bind_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(descriptor_set->GetLayout().GetName());
        AssertThrowMsg(m_bind_index != ~0u, "Invalid bind index for descriptor set %s", descriptor_set->GetLayout().GetName().LookupString());
    }

    BindDescriptorSet(const DescriptorSetRef& descriptor_set, const RaytracingPipelineRef& pipeline, const ArrayMap<Name, uint32>& offsets, uint32 bind_index)
        : m_descriptor_set(descriptor_set),
          m_pipeline(pipeline),
          m_offsets(offsets),
          m_bind_index(bind_index)
    {
        AssertThrowMsg(descriptor_set != nullptr, "Descriptor set must not be null");
        AssertThrowMsg(descriptor_set->IsCreated(), "Descriptor set is not created yet");
        AssertThrowMsg(m_bind_index != ~0u, "Invalid bind index");
    }

    HYP_API virtual void Prepare(FrameBase* frame) override;

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline.Visit([this, &cmd](const auto& pipeline)
            {
                m_descriptor_set->Bind(cmd, pipeline, m_offsets, m_bind_index);
            });
    }

private:
    DescriptorSetRef m_descriptor_set;
    Variant<GraphicsPipelineRef, ComputePipelineRef, RaytracingPipelineRef> m_pipeline;
    ArrayMap<Name, uint32> m_offsets;
    uint32 m_bind_index;
};

class BindDescriptorTable final : public CmdBase
{
public:
    BindDescriptorTable(const DescriptorTableRef& descriptor_table, const GraphicsPipelineRef& graphics_pipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets, uint32 frame_index)
        : m_descriptor_table(descriptor_table),
          m_pipeline(graphics_pipeline),
          m_offsets(offsets),
          m_frame_index(frame_index)
    {
        AssertThrowMsg(descriptor_table != nullptr, "Descriptor table must not be null");
    }

    BindDescriptorTable(const DescriptorTableRef& descriptor_table, const ComputePipelineRef& compute_pipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets, uint32 frame_index)
        : m_descriptor_table(descriptor_table),
          m_pipeline(compute_pipeline),
          m_offsets(offsets),
          m_frame_index(frame_index)
    {
        AssertThrowMsg(descriptor_table != nullptr, "Descriptor table must not be null");
    }

    BindDescriptorTable(const DescriptorTableRef& descriptor_table, const RaytracingPipelineRef& raytracing_pipeline, const ArrayMap<Name, ArrayMap<Name, uint32>>& offsets, uint32 frame_index)
        : m_descriptor_table(descriptor_table),
          m_pipeline(raytracing_pipeline),
          m_offsets(offsets),
          m_frame_index(frame_index)
    {
        AssertThrowMsg(descriptor_table != nullptr, "Descriptor table must not be null");
    }

    HYP_API virtual void Prepare(FrameBase* frame) override;

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline.Visit([this, &cmd](const auto& pipeline)
            {
                m_descriptor_table->Bind(cmd, m_frame_index, pipeline, m_offsets);
            });
    }

private:
    DescriptorTableRef m_descriptor_table;
    Variant<GraphicsPipelineRef, ComputePipelineRef, RaytracingPipelineRef> m_pipeline;
    ArrayMap<Name, ArrayMap<Name, uint32>> m_offsets;
    uint32 m_frame_index;
};

class InsertBarrier final : public CmdBase
{
public:
    InsertBarrier(const GpuBufferRef& buffer, const ResourceState& state, ShaderModuleType shader_module_type = SMT_UNSET)
        : m_buffer(buffer),
          m_state(state),
          m_shader_module_type(shader_module_type)
    {
    }

    InsertBarrier(const ImageRef& image, const ResourceState& state, ShaderModuleType shader_module_type = SMT_UNSET)
        : m_image(image),
          m_state(state),
          m_shader_module_type(shader_module_type)
    {
    }

    InsertBarrier(const ImageRef& image, const ResourceState& state, const ImageSubResource& sub_resource, ShaderModuleType shader_module_type = SMT_UNSET)
        : m_image(image),
          m_state(state),
          m_sub_resource(sub_resource),
          m_shader_module_type(shader_module_type)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        if (m_buffer)
        {
            m_buffer->InsertBarrier(cmd, m_state, m_shader_module_type);
        }
        else if (m_image)
        {
            if (m_sub_resource)
            {
                m_image->InsertBarrier(cmd, *m_sub_resource, m_state, m_shader_module_type);
            }
            else
            {
                m_image->InsertBarrier(cmd, m_state, m_shader_module_type);
            }
        }
    }

private:
    GpuBufferRef m_buffer;
    ImageRef m_image;
    ResourceState m_state;
    Optional<ImageSubResource> m_sub_resource;
    ShaderModuleType m_shader_module_type;
};

class Blit final : public CmdBase
{
public:
    Blit(const ImageRef& src_image, const ImageRef& dst_image)
        : m_src_image(src_image),
          m_dst_image(dst_image)
    {
    }

    Blit(const ImageRef& src_image, const ImageRef& dst_image, const Rect<uint32>& src_rect, const Rect<uint32>& dst_rect)
        : m_src_image(src_image),
          m_dst_image(dst_image),
          m_src_rect(src_rect),
          m_dst_rect(dst_rect)
    {
    }

    Blit(const ImageRef& src_image, const ImageRef& dst_image, const Rect<uint32>& src_rect, const Rect<uint32>& dst_rect, uint32 src_mip, uint32 dst_mip, uint32 src_face, uint32 dst_face)
        : m_src_image(src_image),
          m_dst_image(dst_image),
          m_src_rect(src_rect),
          m_dst_rect(dst_rect),
          m_mip_face_info(MipFaceInfo { src_mip, dst_mip, src_face, dst_face })
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        if (m_mip_face_info)
        {
            MipFaceInfo info = *m_mip_face_info;

            if (m_src_rect && m_dst_rect)
            {
                m_dst_image->Blit(cmd, m_src_image, *m_src_rect, *m_dst_rect, info.src_mip, info.dst_mip, info.src_face, info.dst_face);
            }
            else
            {
                m_dst_image->Blit(cmd, m_src_image, info.src_mip, info.dst_mip, info.src_face, info.dst_face);
            }
        }
        else
        {
            if (m_src_rect && m_dst_rect)
            {
                m_dst_image->Blit(cmd, m_src_image, *m_src_rect, *m_dst_rect);
            }
            else
            {
                m_dst_image->Blit(cmd, m_src_image);
            }
        }
    }

private:
    struct MipFaceInfo
    {
        uint32 src_mip;
        uint32 dst_mip;
        uint32 src_face;
        uint32 dst_face;
    };

    ImageRef m_src_image;
    ImageRef m_dst_image;

    Optional<Rect<uint32>> m_src_rect;
    Optional<Rect<uint32>> m_dst_rect;

    Optional<MipFaceInfo> m_mip_face_info;
};

class BlitRect final : public CmdBase
{
public:
    BlitRect(const ImageRef& src_image, const ImageRef& dst_image, const Rect<uint32>& src_rect, const Rect<uint32>& dst_rect)
        : m_src_image(src_image),
          m_dst_image(dst_image),
          m_src_rect(src_rect),
          m_dst_rect(dst_rect)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_dst_image->Blit(cmd, m_src_image, m_src_rect, m_dst_rect);
    }

private:
    ImageRef m_src_image;
    ImageRef m_dst_image;
    Rect<uint32> m_src_rect;
    Rect<uint32> m_dst_rect;
};

class CopyImageToBuffer final : public CmdBase
{
public:
    CopyImageToBuffer(const ImageRef& image, const GpuBufferRef& buffer)
        : m_image(image),
          m_buffer(buffer)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_image->CopyToBuffer(cmd, m_buffer);
    }

private:
    ImageRef m_image;
    GpuBufferRef m_buffer;
};

class CopyBufferToImage final : public CmdBase
{
public:
    CopyBufferToImage(const GpuBufferRef& buffer, const ImageRef& image)
        : m_buffer(buffer),
          m_image(image)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_image->CopyFromBuffer(cmd, m_buffer);
    }

private:
    GpuBufferRef m_buffer;
    ImageRef m_image;
};

class CopyBuffer final : public CmdBase
{
public:
    CopyBuffer(const GpuBufferRef& src_buffer, const GpuBufferRef& dst_buffer, SizeType size)
        : m_src_buffer(src_buffer),
          m_dst_buffer(dst_buffer),
          m_size(size)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_dst_buffer->CopyFrom(cmd, m_src_buffer, m_size);
    }

private:
    GpuBufferRef m_src_buffer;
    GpuBufferRef m_dst_buffer;
    SizeType m_size;
};

class GenerateMipmaps final : public CmdBase
{
public:
    GenerateMipmaps(const ImageRef& image)
        : m_image(image)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_image->GenerateMipmaps(cmd);
    }

private:
    ImageRef m_image;
};

class DispatchCompute final : public CmdBase
{
public:
    DispatchCompute(const ComputePipelineRef& pipeline, Vec3u workgroup_count)
        : m_pipeline(pipeline),
          m_workgroup_count(workgroup_count)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline->Dispatch(cmd, m_workgroup_count);
    }

private:
    ComputePipelineRef m_pipeline;
    Vec3u m_workgroup_count;
};

class TraceRays final : public CmdBase
{
public:
    TraceRays(const RaytracingPipelineRef& pipeline, const Vec3u& workgroup_count)
        : m_pipeline(pipeline),
          m_workgroup_count(workgroup_count)
    {
    }

    virtual void Execute(const CommandBufferRef& cmd) override
    {
        m_pipeline->TraceRays(cmd, m_workgroup_count);
    }

private:
    RaytracingPipelineRef m_pipeline;
    Vec3u m_workgroup_count;
};

class CmdList
{
public:
    CmdList();
    CmdList(const CmdList& other) = delete;
    CmdList& operator=(const CmdList& other) = delete;
    CmdList(CmdList&& other) noexcept = delete;
    CmdList& operator=(CmdList&& other) noexcept = delete;
    ~CmdList();

    HYP_FORCE_INLINE const Array<CmdBase*>& GetCommands() const
    {
        return m_commands;
    }

    template <class CmdType, class... Args>
    void Add(Args&&... args)
    {
        static CmdMemoryPool<CmdType>& pool = CmdMemoryPool<CmdType>::GetInstance();

        m_commands.PushBack(pool.NewCommand(std::forward<Args>(args)...));
    }

    void Concat(CmdList&& other)
    {
        m_commands.Concat(std::move(other.m_commands));
    }

    void Prepare(FrameBase* frame);
    void Execute(const CommandBufferRef& cmd);

private:
    void FreeCommand(CmdBase* command);

    Array<CmdBase*> m_commands;
};

} // namespace hyperion

#endif

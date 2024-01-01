#ifndef HYPERION_V2_FRAMEBUFFER_H
#define HYPERION_V2_FRAMEBUFFER_H

#include <core/Base.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::AttachmentUsage;
using renderer::Attachment;
using renderer::RenderPass;
using renderer::RenderPassStage;
using renderer::RenderPassMode;
using renderer::LoadOperation;
using renderer::StoreOperation;

struct AttachmentDef
{
    AttachmentRef attachment;
    AttachmentUsageRef attachment_usage;
    LoadOperation load_op;
    StoreOperation store_op;
};

struct AttachmentMap
{
    using Iterator      = typename FlatMap<UInt, AttachmentDef>::Iterator;
    using ConstIterator = typename FlatMap<UInt, AttachmentDef>::ConstIterator;

    FlatMap<UInt, AttachmentDef> attachments;

    void AddAttachment(
        UInt binding,
        ImageRef &&image,
        RenderPassStage stage,
        LoadOperation load_op,
        StoreOperation store_op
    )
    {
        attachments.Set(
            binding,
            AttachmentDef {
                MakeRenderObject<Attachment>(
                    std::move(image),
                    stage
                ),
                AttachmentUsageRef::unset,
                load_op,
                store_op
            }
        );
    }

    ~AttachmentMap();

    SizeType Size() const
        { return attachments.Size(); }

    AttachmentDef &At(UInt binding)
        { return attachments.At(binding); }

    const AttachmentDef &At(UInt binding) const
        { return attachments.At(binding); }

    HYP_DEF_STL_BEGIN_END(
        attachments.Begin(),
        attachments.End()
    )
};

class Framebuffer
    : public BasicObject<STUB_CLASS(Framebuffer)>
{
public:
    Framebuffer(
        Extent2D extent,
        RenderPassStage stage = RenderPassStage::SHADER,
        RenderPassMode render_pass_mode = RenderPassMode::RENDER_PASS_INLINE,
        UInt num_multiview_layers = 0
    );

    Framebuffer(
        Extent3D extent,
        RenderPassStage stage = RenderPassStage::SHADER,
        RenderPassMode render_pass_mode = RenderPassMode::RENDER_PASS_INLINE,
        UInt num_multiview_layers = 0
    );

    Framebuffer(const Framebuffer &other) = delete;
    Framebuffer &operator=(const Framebuffer &other) = delete;
    ~Framebuffer();

    void AddAttachment(
        UInt binding,
        ImageRef &&image,
        RenderPassStage stage,
        LoadOperation load_op,
        StoreOperation store_op
    )
    {
        m_attachment_map.AddAttachment(
            binding,
            std::move(image),
            stage,
            load_op,
            store_op
        );
    }

    void AddAttachmentUsage(AttachmentUsage *attachment);
    void RemoveAttachmentUsage(const Attachment *attachment);

    const AttachmentMap &GetAttachmentMap() const
        { return m_attachment_map; }

    auto &GetAttachmentUsages()
        { return m_render_pass->GetAttachmentUsages(); }

    const auto &GetAttachmentUsages() const
        { return m_render_pass->GetAttachmentUsages(); }

    const FramebufferObjectRef &GetFramebuffer(UInt frame_index) const
        { return m_framebuffers[frame_index]; }

    const RenderPassRef &GetRenderPass() const { return m_render_pass; }

    Extent2D GetExtent() const
        { return Extent2D(m_extent); }

    void Init();

    void BeginCapture(UInt frame_index, CommandBuffer *command_buffer);
    void EndCapture(UInt frame_index, CommandBuffer *command_buffer);

private:
    AttachmentMap                                           m_attachment_map;
    FixedArray<FramebufferObjectRef, max_frames_in_flight>  m_framebuffers;
    RenderPassRef                                           m_render_pass;
    Extent3D                                                m_extent;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FRAMEBUFFER_H

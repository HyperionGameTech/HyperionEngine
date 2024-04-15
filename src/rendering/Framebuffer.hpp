/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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
    AttachmentRef       attachment;
    AttachmentUsageRef  attachment_usage;
    LoadOperation       load_op;
    StoreOperation      store_op;
};

struct HYP_API AttachmentMap
{
    using Iterator      = typename FlatMap<uint, AttachmentDef>::Iterator;
    using ConstIterator = typename FlatMap<uint, AttachmentDef>::ConstIterator;

    FlatMap<uint, AttachmentDef> attachments;

    void AddAttachment(
        uint binding,
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

    AttachmentDef &At(uint binding)
        { return attachments.At(binding); }

    const AttachmentDef &At(uint binding) const
        { return attachments.At(binding); }

    HYP_DEF_STL_BEGIN_END(
        attachments.Begin(),
        attachments.End()
    )
};

class HYP_API Framebuffer
    : public BasicObject<STUB_CLASS(Framebuffer)>
{
public:
    Framebuffer(
        Extent2D extent,
        RenderPassStage stage = RenderPassStage::SHADER,
        RenderPassMode render_pass_mode = RenderPassMode::RENDER_PASS_INLINE,
        uint num_multiview_layers = 0
    );

    Framebuffer(
        Extent3D extent,
        RenderPassStage stage = RenderPassStage::SHADER,
        RenderPassMode render_pass_mode = RenderPassMode::RENDER_PASS_INLINE,
        uint num_multiview_layers = 0
    );

    Framebuffer(const Framebuffer &other) = delete;
    Framebuffer &operator=(const Framebuffer &other) = delete;
    ~Framebuffer();

    void AddAttachment(
        uint binding,
        ImageRef &&image,
        RenderPassStage stage,
        LoadOperation load_op,
        StoreOperation store_op
    )
    {
        m_attachment_map->AddAttachment(
            binding,
            std::move(image),
            stage,
            load_op,
            store_op
        );
    }

    void AddAttachmentUsage(AttachmentUsageRef attachment);
    void RemoveAttachmentUsage(const AttachmentRef &attachment);

    const RC<AttachmentMap> &GetAttachmentMap() const
        { return m_attachment_map; }

    auto &GetAttachmentUsages()
        { return m_render_pass->GetAttachmentUsages(); }

    const auto &GetAttachmentUsages() const
        { return m_render_pass->GetAttachmentUsages(); }

    const FramebufferObjectRef &GetFramebuffer(uint frame_index) const
        { return m_framebuffers[frame_index]; }

    const RenderPassRef &GetRenderPass() const { return m_render_pass; }

    Extent2D GetExtent() const
        { return Extent2D(m_extent); }

    void Init();

    void BeginCapture(uint frame_index, CommandBuffer *command_buffer);
    void EndCapture(uint frame_index, CommandBuffer *command_buffer);

private:
    RC<AttachmentMap>                                       m_attachment_map;
    FixedArray<FramebufferObjectRef, max_frames_in_flight>  m_framebuffers;
    RenderPassRef                                           m_render_pass;
    Extent3D                                                m_extent;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FRAMEBUFFER_H

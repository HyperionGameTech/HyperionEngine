/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_FBO_HPP
#define HYPERION_BACKEND_RENDERER_FBO_HPP

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/Platform.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
struct AttachmentDef
{
    static constexpr PlatformType platform = PLATFORM;
    
    ImageRef<PLATFORM>      image;
    AttachmentRef<PLATFORM> attachment;
};

template <PlatformType PLATFORM>
struct AttachmentMap
{
    static constexpr PlatformType platform = PLATFORM;
    
    using Iterator = typename FlatMap<uint32, AttachmentDef<PLATFORM>>::Iterator;
    using ConstIterator = typename FlatMap<uint32, AttachmentDef<PLATFORM>>::ConstIterator;

    FlatMap<uint32, AttachmentDef<PLATFORM>>  attachments;

    ~AttachmentMap()
    {
        Reset();
    }

    RendererResult Create(Device<PLATFORM> *device);
    RendererResult Resize(Device<PLATFORM> *device, Vec2u new_size);

    void Reset()
    {
        for (auto &it : attachments) {
            SafeRelease(std::move(it.second.attachment));
        }

        attachments.Clear();
    }

    HYP_FORCE_INLINE SizeType Size() const
        { return attachments.Size(); }

    HYP_FORCE_INLINE const AttachmentRef<PLATFORM> &GetAttachment(uint32 binding) const
    {
        const auto it = attachments.Find(binding);

        if (it == attachments.End()) {
            return AttachmentRef<PLATFORM>::Null();
        }

        return it->second.attachment;
    }

    HYP_FORCE_INLINE AttachmentRef<PLATFORM> AddAttachment(const AttachmentRef<PLATFORM> &attachment)
    {
        AssertThrow(attachment.IsValid());
        AssertThrow(attachment->GetImage().IsValid());

        AssertThrowMsg(attachment->HasBinding(), "Attachment must have a binding");

        const uint32 binding = attachment->GetBinding();
        AssertThrowMsg(!attachments.Contains(binding), "Attachment already exists at binding: %u", binding);

        attachments.Set(
            binding,
            AttachmentDef<PLATFORM> {
                attachment->GetImage(),
                attachment
            }
        );

        return attachment;
    }

    HYP_FORCE_INLINE AttachmentRef<PLATFORM> AddAttachment(
        uint32 binding,
        Vec2u extent,
        InternalFormat format,
        ImageType type,
        RenderPassStage stage,
        LoadOperation load_op,
        StoreOperation store_op
    )
    {
        ImageRef<PLATFORM> image = MakeRenderObject<Image<PLATFORM>>(
            TextureDesc
            {
                type,
                format,
                Vec3u { extent.x, extent.y, 1 }
            }
        );

        image->SetIsAttachmentTexture(true);

        AttachmentRef<PLATFORM> attachment = MakeRenderObject<Attachment<PLATFORM>>(
            image,
            stage
        );

        attachments.Set(
            binding,
            AttachmentDef<PLATFORM> {
                image,
                attachment
            }
        );

        return attachment;
    }

    HYP_DEF_STL_BEGIN_END(
        attachments.Begin(),
        attachments.End()
    )
};

template <PlatformType PLATFORM>
struct FramebufferPlatformImpl;

template <PlatformType PLATFORM>
class Framebuffer
{
public:
    static constexpr PlatformType platform = PLATFORM;
    
    HYP_API Framebuffer(
        Vec2u extent,
        RenderPassStage stage = RenderPassStage::SHADER,
        RenderPassMode render_pass_mode = RenderPassMode::RENDER_PASS_INLINE,
        uint32 num_multiview_layers = 0
    );

    Framebuffer(const Framebuffer &other)               = delete;
    Framebuffer &operator=(const Framebuffer &other)    = delete;
    HYP_API ~Framebuffer();

    HYP_FORCE_INLINE FramebufferPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_FORCE_INLINE const FramebufferPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_FORCE_INLINE uint32 GetWidth() const
        { return m_extent.x; }

    HYP_FORCE_INLINE uint32 GetHeight() const
        { return m_extent.y; }

    HYP_FORCE_INLINE const Vec2u &GetExtent() const
        { return m_extent; }

    HYP_FORCE_INLINE const RenderPassRef<PLATFORM> &GetRenderPass() const
        { return m_render_pass; }

    HYP_FORCE_INLINE AttachmentRef<PLATFORM> AddAttachment(const AttachmentRef<PLATFORM> &attachment)
        { return m_attachment_map.AddAttachment(attachment); }

    HYP_FORCE_INLINE AttachmentRef<PLATFORM> AddAttachment(
        uint32 binding,
        InternalFormat format,
        ImageType type,
        RenderPassStage stage,
        LoadOperation load_op,
        StoreOperation store_op
    )
    {
        return m_attachment_map.AddAttachment(
            binding,
            m_extent,
            format,
            type,
            stage,
            load_op,
            store_op
        );
    }

    HYP_API bool RemoveAttachment(uint32 binding);

    HYP_FORCE_INLINE const AttachmentRef<PLATFORM> &GetAttachment(uint32 binding) const
        { return m_attachment_map.GetAttachment(binding); }

    HYP_FORCE_INLINE const AttachmentMap<PLATFORM> &GetAttachmentMap() const
        { return m_attachment_map; }

    HYP_API bool IsCreated() const;

    HYP_API RendererResult Create(Device<PLATFORM> *device);
    HYP_API RendererResult Destroy(Device<PLATFORM> *device);

    HYP_API RendererResult Resize(Device<PLATFORM> *device, Vec2u new_size);

    HYP_API void BeginCapture(CommandBuffer<PLATFORM> *command_buffer, uint32 frame_index);
    HYP_API void EndCapture(CommandBuffer<PLATFORM> *command_buffer, uint32 frame_index);

private:
    FramebufferPlatformImpl<PLATFORM>   m_platform_impl;

    Vec2u                               m_extent;

    RenderPassRef<PLATFORM>             m_render_pass;
    AttachmentMap<PLATFORM>             m_attachment_map;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFramebuffer.hpp>
#else
#error Unsupported rendering backend
#endif


namespace hyperion {
namespace renderer {

using Framebuffer = platform::Framebuffer<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif
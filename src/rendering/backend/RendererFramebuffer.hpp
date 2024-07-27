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
    
    using Iterator = typename FlatMap<uint, AttachmentDef<PLATFORM>>::Iterator;
    using ConstIterator = typename FlatMap<uint, AttachmentDef<PLATFORM>>::ConstIterator;

    FlatMap<uint, AttachmentDef<PLATFORM>>  attachments;

    ~AttachmentMap()
    {
        Reset();
    }

    Result Create(Device<PLATFORM> *device);
    Result Resize(Device<PLATFORM> *device, Extent2D new_size);

    void Reset()
    {
        for (auto &it : attachments) {
            SafeRelease(std::move(it.second.attachment));
        }

        attachments.Clear();
    }

    HYP_FORCE_INLINE SizeType Size() const
        { return attachments.Size(); }

    HYP_FORCE_INLINE const AttachmentRef<PLATFORM> &GetAttachment(uint binding) const
    {
        const auto it = attachments.Find(binding);

        if (it == attachments.End()) {
            return AttachmentRef<PLATFORM>::Null();
        }

        return it->second.attachment;
    }

    HYP_FORCE_INLINE void AddAttachment(const AttachmentRef<PLATFORM> &attachment)
    {
        AssertThrow(attachment.IsValid());
        AssertThrow(attachment->GetImage().IsValid());

        AssertThrowMsg(attachment->HasBinding(), "Attachment must have a binding");

        const uint binding = attachment->GetBinding();
        AssertThrowMsg(!attachments.Contains(binding), "Attachment already exists at binding: %u", binding);

        attachments.Set(
            binding,
            AttachmentDef<PLATFORM> {
                attachment->GetImage(),
                attachment
            }
        );
    }

    HYP_FORCE_INLINE void AddAttachment(
        uint binding,
        Extent2D extent,
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
                Extent3D(extent.width, extent.height, 1)
            }
        );

        image->SetIsAttachmentTexture(true);

        attachments.Set(
            binding,
            AttachmentDef<PLATFORM> {
                image,
                MakeRenderObject<Attachment<PLATFORM>>(
                    image,
                    stage
                )
            }
        );
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
        Extent2D extent,
        RenderPassStage stage = RenderPassStage::SHADER,
        RenderPassMode render_pass_mode = RenderPassMode::RENDER_PASS_INLINE,
        uint num_multiview_layers = 0
    );

    Framebuffer(const Framebuffer &other)               = delete;
    Framebuffer &operator=(const Framebuffer &other)    = delete;
    HYP_API ~Framebuffer();

    HYP_FORCE_INLINE FramebufferPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_FORCE_INLINE const FramebufferPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_FORCE_INLINE uint GetWidth() const
        { return m_extent.width; }

    HYP_FORCE_INLINE uint GetHeight() const
        { return m_extent.height; }

    HYP_FORCE_INLINE Extent2D GetExtent() const
        { return m_extent; }

    HYP_FORCE_INLINE const RenderPassRef<PLATFORM> &GetRenderPass() const
        { return m_render_pass; }

    HYP_FORCE_INLINE void AddAttachment(const AttachmentRef<PLATFORM> &attachment)
        { m_attachment_map.AddAttachment(attachment); }

    HYP_FORCE_INLINE void AddAttachment(
        uint binding,
        InternalFormat format,
        ImageType type,
        RenderPassStage stage,
        LoadOperation load_op,
        StoreOperation store_op
    )
    {
        m_attachment_map.AddAttachment(
            binding,
            m_extent,
            format,
            type,
            stage,
            load_op,
            store_op
        );
    }

    HYP_API bool RemoveAttachment(uint binding);

    HYP_FORCE_INLINE const AttachmentRef<PLATFORM> &GetAttachment(uint binding) const
        { return m_attachment_map.GetAttachment(binding); }

    HYP_FORCE_INLINE const AttachmentMap<PLATFORM> &GetAttachmentMap() const
        { return m_attachment_map; }

    HYP_API bool IsCreated() const;

    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Destroy(Device<PLATFORM> *device);

    HYP_API Result Resize(Device<PLATFORM> *device, Extent2D new_size);

    HYP_API void BeginCapture(CommandBuffer<PLATFORM> *command_buffer, uint frame_index);
    HYP_API void EndCapture(CommandBuffer<PLATFORM> *command_buffer, uint frame_index);

private:
    FramebufferPlatformImpl<PLATFORM>   m_platform_impl;

    Extent2D                            m_extent;

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
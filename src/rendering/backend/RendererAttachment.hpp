/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_ATTACHMENT_H
#define HYPERION_V2_BACKEND_RENDERER_ATTACHMENT_H

#include <core/Defines.hpp>
#include <core/lib/DynArray.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/Platform.hpp>
#include <math/Extent.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

enum class RenderPassStage
{
    NONE,
    PRESENT, /* for presentation on screen */
    SHADER   /* for use as a sampled texture in a shader */
};

enum class LoadOperation
{
    UNDEFINED,
    NONE,
    CLEAR,
    LOAD
};

enum class StoreOperation
{
    UNDEFINED,
    NONE,
    STORE
};

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class Attachment
{
public:
    HYP_API Attachment(ImageRef<PLATFORM> image, RenderPassStage stage);
    Attachment(const Attachment &other)             = delete;
    Attachment &operator=(const Attachment &other)  = delete;
    HYP_API ~Attachment();

    const ImageRef<PLATFORM> &GetImage() const
        { return m_image; }

    RenderPassStage GetRenderPassStage() const
        { return m_stage; }

    InternalFormat GetFormat() const
        { return m_image ? m_image->GetTextureFormat() : InternalFormat::NONE; }

    bool IsDepthAttachment() const
        { return m_image ? m_image->IsDepthStencil() : false; }

    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Destroy(Device<PLATFORM> *device);

private:
    ImageRef<PLATFORM>  m_image;
    RenderPassStage     m_stage;
};

template <PlatformType PLATFORM>
class AttachmentUsage
{
public:
    HYP_API AttachmentUsage(
        AttachmentRef<PLATFORM> attachment,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE,
        BlendFunction blend_function = BlendFunction::None()
    );

    HYP_API AttachmentUsage(
        AttachmentRef<PLATFORM> attachment,
        ImageViewRef<PLATFORM> image_view,
        SamplerRef<PLATFORM> sampler,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE,
        BlendFunction blend_function = BlendFunction::None()
    );

    AttachmentUsage(const AttachmentUsage &other)               = delete;
    AttachmentUsage &operator=(const AttachmentUsage &other)    = delete;
    HYP_API ~AttachmentUsage();

    const AttachmentRef<PLATFORM> &GetAttachment() const
        { return m_attachment; }

    const ImageViewRef<PLATFORM> &GetImageView() const
        { return m_image_view; }

    const SamplerRef<PLATFORM> &GetSampler() const
        { return m_sampler; }

    LoadOperation GetLoadOperation() const
        { return m_load_operation; }

    StoreOperation GetStoreOperation() const
        { return m_store_operation; }

    const BlendFunction &GetBlendFunction() const
        { return m_blend_function; }

    void SetBlendFunction(const BlendFunction &blend_function)
        { m_blend_function = blend_function; }

    uint GetBinding() const
        { return m_binding; }
    
    void SetBinding(uint binding)
        { m_binding = binding; }

    bool HasBinding() const 
        { return m_binding != MathUtil::MaxSafeValue<uint>(); }

    InternalFormat GetFormat() const;
    bool IsDepthAttachment() const;

    bool AllowBlending() const
        { return m_allow_blending; }

    void SetAllowBlending(bool allow_blending)
        { m_allow_blending = allow_blending; }
    
    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Destroy(Device<PLATFORM> *device);

private:
    AttachmentRef<PLATFORM> m_attachment;
    ImageViewRef<PLATFORM>  m_image_view;
    SamplerRef<PLATFORM>    m_sampler;
    
    LoadOperation           m_load_operation;
    StoreOperation          m_store_operation;

    BlendFunction           m_blend_function;

    uint                    m_binding = MathUtil::MaxSafeValue<uint>();

    bool                    m_image_view_owned = false;
    bool                    m_sampler_owned = false;

    bool                    m_allow_blending = true;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererAttachment.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Attachment = platform::Attachment<Platform::CURRENT>;
using AttachmentUsage = platform::AttachmentUsage<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif
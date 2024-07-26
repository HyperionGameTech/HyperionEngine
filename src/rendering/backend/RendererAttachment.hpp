/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_ATTACHMENT_HPP
#define HYPERION_BACKEND_RENDERER_ATTACHMENT_HPP

#include <core/Defines.hpp>
#include <core/containers/Array.hpp>
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
struct AttachmentPlatformImpl;

template <PlatformType PLATFORM>
class Attachment
{
public:
    static constexpr PlatformType platform = PLATFORM;
    
    HYP_API Attachment(
        const ImageRef<PLATFORM> &image,
        RenderPassStage stage,
        LoadOperation load_operation = LoadOperation::CLEAR,
        StoreOperation store_operation = StoreOperation::STORE,
        BlendFunction blend_function = BlendFunction::None()
    );
    Attachment(const Attachment &other)             = delete;
    Attachment &operator=(const Attachment &other)  = delete;
    HYP_API ~Attachment();

    HYP_FORCE_INLINE AttachmentPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_FORCE_INLINE const AttachmentPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_FORCE_INLINE const ImageRef<PLATFORM> &GetImage() const
        { return m_image; }

    HYP_FORCE_INLINE const ImageViewRef<PLATFORM> &GetImageView() const
        { return m_image_view; }

    HYP_FORCE_INLINE RenderPassStage GetRenderPassStage() const
        { return m_stage; }

    HYP_FORCE_INLINE InternalFormat GetFormat() const
        { return m_image ? m_image->GetTextureFormat() : InternalFormat::NONE; }

    HYP_FORCE_INLINE bool IsDepthAttachment() const
        { return m_image ? m_image->IsDepthStencil() : false; }

    HYP_FORCE_INLINE LoadOperation GetLoadOperation() const
        { return m_load_operation; }

    HYP_FORCE_INLINE StoreOperation GetStoreOperation() const
        { return m_store_operation; }

    HYP_FORCE_INLINE const BlendFunction &GetBlendFunction() const
        { return m_blend_function; }

    HYP_FORCE_INLINE void SetBlendFunction(const BlendFunction &blend_function)
        { m_blend_function = blend_function; }

    HYP_FORCE_INLINE uint GetBinding() const
        { return m_binding; }
    
    HYP_FORCE_INLINE void SetBinding(uint binding)
        { m_binding = binding; }

    HYP_FORCE_INLINE bool HasBinding() const 
        { return m_binding != MathUtil::MaxSafeValue<uint>(); }

    HYP_FORCE_INLINE bool AllowBlending() const
        { return m_allow_blending; }

    HYP_FORCE_INLINE void SetAllowBlending(bool allow_blending)
        { m_allow_blending = allow_blending; }

    HYP_API bool IsCreated() const;

    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Destroy(Device<PLATFORM> *device);

private:
    AttachmentPlatformImpl<PLATFORM>    m_platform_impl;

    ImageRef<PLATFORM>                  m_image;
    ImageViewRef<PLATFORM>              m_image_view;

    RenderPassStage                     m_stage;
    
    LoadOperation                       m_load_operation;
    StoreOperation                      m_store_operation;

    BlendFunction                       m_blend_function;

    uint                                m_binding = MathUtil::MaxSafeValue<uint>();

    bool                                m_allow_blending = true;
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

} // namespace renderer
} // namespace hyperion

#endif
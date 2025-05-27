/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_TEXTURE_HPP
#define HYPERION_RENDER_TEXTURE_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/containers/Bitset.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/RenderResource.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class Texture;

class TextureRenderResource final : public RenderResourceBase
{
public:
    TextureRenderResource(Texture* texture);
    virtual ~TextureRenderResource() override;

    HYP_FORCE_INLINE Texture* GetTexture() const
    {
        return m_texture;
    }

    HYP_FORCE_INLINE const ImageRef& GetImage() const
    {
        return m_image;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetImageView() const
    {
        return m_image_view;
    }

    /*! \brief Enqueues a render command to generate mipmaps for the texture and waits for it to finish.
     *  Thread-safe, blocking function. Use sparingly. */
    void RenderMipmaps();

    /*! \brief Enqueues a render command to copy the texture data to a buffer and waits for it to finish.
     *  Thread-safe, blocking function. Use sparingly. */
    void Readback(ByteBuffer& out_byte_buffer);

    void Resize(const Vec3u& extent);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase* GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    Texture* m_texture;

    ImageRef m_image;
    ImageViewRef m_image_view;
};

} // namespace hyperion

#endif
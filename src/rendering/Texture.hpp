#ifndef HYPERION_V2_TEXTURE_H
#define HYPERION_V2_TEXTURE_H

#include <core/Base.hpp>
#include <core/Handle.hpp>
#include <core/lib/UniquePtr.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>

#include <Types.hpp>

#include <memory>
#include <map>

namespace hyperion::v2 {

using renderer::Image;
using renderer::TextureImage;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Extent2D;
using renderer::Extent3D;
using renderer::CommandBuffer;

class Texture
    : public EngineComponentBase<STUB_CLASS(Texture)>,
      public RenderResource
{
public:
    Texture(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode,
        WrapMode wrap_mode,
        const UByte *bytes
    );

    Texture(
        Image &&image,
        FilterMode filter_mode,
        WrapMode wrap_mode
    );

    Texture(const Texture &other) = delete;
    Texture &operator=(const Texture &other) = delete;

    Texture(Texture &&other) noexcept;
    Texture &operator=(Texture &&other) noexcept = delete;

    ~Texture();
    
    ImageRef &GetImage() { return m_image; }
    const ImageRef &GetImage() const { return m_image; }

    ImageViewRef &GetImageView() { return m_image_view; }
    const ImageViewRef &GetImageView() const { return m_image_view; }

    SamplerRef &GetSampler() { return m_sampler; }
    const SamplerRef &GetSampler() const { return m_sampler; }

    ImageType GetType() const { return m_image->GetType(); }

    UInt NumFaces() const { return m_image->NumFaces(); }

    bool IsTextureCube() const { return m_image->IsTextureCube(); }
    bool IsPanorama() const { return m_image->IsPanorama(); }

    const Extent3D &GetExtent() const { return m_image->GetExtent(); }

    InternalFormat GetFormat() const { return m_image->GetTextureFormat(); }
    FilterMode GetFilterMode() const { return m_sampler->GetFilterMode(); }
    WrapMode GetWrapMode() const { return m_sampler->GetWrapMode(); }
    
    void Init();

protected:
    ImageRef m_image;
    ImageViewRef m_image_view;
    SamplerRef m_sampler;
};

class Texture2D : public Texture
{
public:
    Texture2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        WrapMode wrap_mode,
        const UByte *bytes
    ) : Texture(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_2D,
        filter_mode,
        wrap_mode,
        bytes
    )
    {
    }
};

class Texture3D : public Texture
{
public:
    Texture3D(
        Extent3D extent,
        InternalFormat format,
        FilterMode filter_mode,
        WrapMode wrap_mode,
        const UByte *bytes
    ) : Texture(
        extent,
        format,
        ImageType::TEXTURE_TYPE_3D,
        filter_mode,
        wrap_mode,
        bytes
    )
    {
    }
};

class TextureCube : public Texture
{
public:
    TextureCube(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        WrapMode wrap_mode,
        const UByte *bytes
    ) : Texture(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            filter_mode,
            wrap_mode,
            bytes
        )
    {
    }

    TextureCube(std::unique_ptr<Texture> &&other) noexcept
        : Texture(
              other
                ? (other->IsTextureCube()
                    ? other->GetImage()->GetExtent()
                    : (other->IsPanorama()
                        ? Extent3D(other->GetImage()->GetExtent().width / 4, other->GetImage()->GetExtent().height / 3, 1)
                        : (other->GetImage()->GetExtent() / Extent3D(6, 6, 1))))
                : Extent3D { 1, 1, 1 },
              other ? other->GetFormat() : InternalFormat::RGBA8,
              ImageType::TEXTURE_TYPE_CUBEMAP,
              other ? other->GetFilterMode() : FilterMode::TEXTURE_FILTER_NEAREST,
              other ? other->GetWrapMode() : WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
              nullptr
          )
    {
        if (other && other->GetImage()->HasAssignedImageData()) {
            m_image->EnsureCapacity(m_image->GetByteSize());
            m_image->CopyImageData(other->GetImage()->GetBytes(), other->GetImage()->GetByteSize(), 0);
        }

        other.reset();
    }

    TextureCube(
        FixedArray<Handle<Texture>, 6> &&texture_faces
    ) : Texture(
            texture_faces[0] ? texture_faces[0]->GetExtent() : Extent3D { },
            texture_faces[0] ? texture_faces[0]->GetFormat() : InternalFormat::RGBA8,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            texture_faces[0] ? texture_faces[0]->GetFilterMode() : FilterMode::TEXTURE_FILTER_NEAREST,
            texture_faces[0] ? texture_faces[0]->GetWrapMode() : WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        )
    {
        m_image->EnsureCapacity(m_image->GetByteSize());

        SizeType offset = 0,
            face_size = 0;

        for (auto &texture : texture_faces) {
            if (texture) {
                face_size = texture->GetExtent().Size() * NumComponents(texture->GetFormat());

                m_image->CopyImageData(texture->GetImage()->GetBytes(), face_size, offset);
            }

            texture.Reset();

            offset += face_size;
        }
    }
};

} // namespace hyperion::v2

#endif
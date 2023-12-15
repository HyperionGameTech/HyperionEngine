#ifndef HYPERION_V2_TEXTURE_H
#define HYPERION_V2_TEXTURE_H

#include <core/Base.hpp>
#include <core/Handle.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/FixedArray.hpp>

#include <streaming/StreamedData.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>

#include <scene/VisibilityState.hpp>

#include <math/Vector3.hpp>

#include <Types.hpp>

#include <memory>
#include <map>

namespace hyperion::v2 {

using renderer::Image;
using renderer::TextureImage;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
;
using renderer::CommandBuffer;

class Texture
    : public EngineComponentBase<STUB_CLASS(Texture)>,
      public RenderResource
{
public:
    static const FixedArray<std::pair<Vector3, Vector3>, 6> cubemap_directions;

    Texture();

    Texture(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode,
        WrapMode wrap_mode,
        UniquePtr<StreamedData> &&streamed_data
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

    ImageType GetType() const { return m_image->GetType(); }

    UInt NumFaces() const { return m_image->NumFaces(); }

    bool IsTextureCube() const { return m_image->IsTextureCube(); }
    bool IsPanorama() const { return m_image->IsPanorama(); }

    const Extent3D &GetExtent() const { return m_image->GetExtent(); }

    InternalFormat GetFormat() const { return m_image->GetTextureFormat(); }
    FilterMode GetFilterMode() const { return m_filter_mode; }
    WrapMode GetWrapMode() const { return m_wrap_mode; }
    
    void Init();

    void GenerateMipmaps();

protected:
    FilterMode m_filter_mode;
    WrapMode m_wrap_mode;

    ImageRef m_image;
    ImageViewRef m_image_view;
};

class Texture2D : public Texture
{
public:
    Texture2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        WrapMode wrap_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : Texture(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_2D,
        filter_mode,
        wrap_mode,
        std::move(streamed_data)
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
        UniquePtr<StreamedData> &&streamed_data
    ) : Texture(
        extent,
        format,
        ImageType::TEXTURE_TYPE_3D,
        filter_mode,
        wrap_mode,
        std::move(streamed_data)
    )
    {
    }
};

class TextureArray : public Texture
{
public:
    TextureArray(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        WrapMode wrap_mode,
        UInt num_layers
    ) : Texture(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
            filter_mode,
            wrap_mode,
            UniquePtr<MemoryStreamedData>::Construct(ByteBuffer(extent.Size() * num_layers * NumComponents(format) * NumBytes(format)))
        )
    {
        m_image->SetNumLayers(num_layers);
    }

    UInt NumLayers() const
    {
        return m_image->NumLayers();
    }

    void SetNumLayers(UInt num_layers)
    {
        m_image->SetNumLayers(num_layers);
    }

    void SetLayer(UInt layer, const ByteBuffer &data)
    {
        AssertThrowMsg(layer < NumLayers(), "layer index out of bounds");
        AssertThrowMsg(data.Size() == m_image->GetExtent().Size() * NumComponents(m_image->GetTextureFormat()), "data size does not match texture size");

        Memory::MemCpy(m_image->GetStreamedData()->Load().Data() + layer * m_image->GetExtent().Size() * NumComponents(m_image->GetTextureFormat()), data.Data(), data.Size());
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
        UniquePtr<StreamedData> &&streamed_data
    ) : Texture(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            filter_mode,
            wrap_mode,
            std::move(streamed_data)
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
            m_image->CopyImageData(other->GetImage()->GetStreamedData()->Load());
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
        SizeType offset = 0,
            face_size = 0;

        for (auto &texture : texture_faces) {
            if (texture) {
                face_size = texture->GetExtent().Size() * NumComponents(texture->GetFormat());

                if (texture->GetImage()->HasAssignedImageData()) {
                    m_image->CopyImageData(texture->GetImage()->GetStreamedData()->Load().Data(), face_size, offset);
                }
            }

            texture.Reset();

            offset += face_size;
        }
    }
};

} // namespace hyperion::v2

#endif
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_TEXTURE_HPP
#define HYPERION_V2_TEXTURE_HPP

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

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>

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

class HYP_API Texture
    : public BasicObject<STUB_CLASS(Texture)>
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
    
    const ImageRef &GetImage() const
        { return m_image; }

    const ImageViewRef &GetImageView() const
        { return m_image_view; }

    ImageType GetType() const
        { return m_image->GetType(); }

    uint NumFaces() const
        { return m_image->NumFaces(); }

    bool IsTextureCube() const
        { return m_image->IsTextureCube(); }
    
    bool IsPanorama() const
        { return m_image->IsPanorama(); }

    const Extent3D &GetExtent() const
        { return m_image->GetExtent(); }

    InternalFormat GetFormat() const
        { return m_image->GetTextureFormat(); }

    FilterMode GetFilterMode() const
        { return m_filter_mode; }

    WrapMode GetWrapMode() const
        { return m_wrap_mode; }
    
    void Init();

    void GenerateMipmaps();

    Vec4f Sample(Vec2f uv) const;

protected:
    FilterMode      m_filter_mode;
    WrapMode        m_wrap_mode;

    ImageRef        m_image;
    ImageViewRef    m_image_view;
};

class HYP_API Texture2D : public Texture
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

    Texture2D(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : Texture(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_2D,
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        std::move(streamed_data)
    )
    {
    }
};

class HYP_API Texture3D : public Texture
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

    Texture3D(
        Extent3D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : Texture(
        extent,
        format,
        ImageType::TEXTURE_TYPE_3D,
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
        std::move(streamed_data)
    )
    {
    }
};

class HYP_API TextureArray : public Texture
{
public:
    TextureArray(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        WrapMode wrap_mode,
        uint num_layers
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

    TextureArray(
        Extent2D extent,
        InternalFormat format,
        uint num_layers
    ) : TextureArray(
            extent,
            format,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            num_layers
        )
    {
    }

    uint NumLayers() const
    {
        return m_image->NumLayers();
    }

    void SetNumLayers(uint num_layers)
    {
        m_image->SetNumLayers(num_layers);
    }

    /*void SetLayer(uint layer, const ByteBuffer &data)
    {
        AssertThrowMsg(layer < NumLayers(), "layer index out of bounds");
        AssertThrowMsg(data.Size() == m_image->GetExtent().Size() * NumComponents(m_image->GetTextureFormat()), "data size does not match texture size");

        Memory::MemCpy(m_image->GetStreamedData()->Load().Data() + layer * m_image->GetExtent().Size() * NumComponents(m_image->GetTextureFormat()), data.Data(), data.Size());
    }    */
};

class HYP_API TextureCube : public Texture
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

    TextureCube(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : Texture(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            FilterMode::TEXTURE_FILTER_LINEAR,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            std::move(streamed_data)
        )
    {
    }

    TextureCube(UniquePtr<Texture> &&other) noexcept
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

        other.Reset();
    }

    TextureCube(FixedArray<Handle<Texture>, 6> &&texture_faces)
        : Texture(
            texture_faces[0] ? texture_faces[0]->GetExtent() : Extent3D { },
            texture_faces[0] ? texture_faces[0]->GetFormat() : InternalFormat::RGBA8,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            texture_faces[0] ? texture_faces[0]->GetFilterMode() : FilterMode::TEXTURE_FILTER_NEAREST,
            texture_faces[0] ? texture_faces[0]->GetWrapMode() : WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        )
    {
        AssertThrow(texture_faces[0] != nullptr);

        m_image->SetTextureFormat(texture_faces[0]->GetFormat());

        const SizeType face_size = texture_faces[0]->GetExtent().Size() * NumComponents(texture_faces[0]->GetFormat()) * NumBytes(texture_faces[0]->GetFormat());

        SizeType offset = 0;

        ByteBuffer result_buffer;
        result_buffer.SetSize(face_size * 6);

        for (auto &texture : texture_faces) {
            if (texture) {
                AssertThrow(offset + face_size <= m_image->GetByteSize());

                if (texture->GetImage()->HasAssignedImageData()) {
                    const ByteBuffer byte_buffer = texture->GetImage()->GetStreamedData()->Load();

                    AssertThrow(byte_buffer.Size() == face_size);

                    Memory::MemCpy(result_buffer.Data() + offset, byte_buffer.Data(), face_size);
                }
            }

            texture.Reset();

            offset += face_size;
        }

        m_image->CopyImageData(result_buffer);
    }
};

} // namespace hyperion::v2

#endif
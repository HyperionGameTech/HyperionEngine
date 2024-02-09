#ifndef HYPERION_V2_FBOM_MARSHALS_TEXTURE_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_TEXTURE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <rendering/Texture.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Texture> : public FBOMObjectMarshalerBase<Texture>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(Texture::GetClass().GetName());
    }

    virtual FBOMResult Serialize(const Texture &in_object, FBOMObject &out) const override
    {
        out.SetProperty("extent.width", FBOMUnsignedInt(), in_object.GetExtent().width);
        out.SetProperty("extent.height", FBOMUnsignedInt(), in_object.GetExtent().height);
        out.SetProperty("extent.depth", FBOMUnsignedInt(), in_object.GetExtent().depth);

        out.SetProperty("format", FBOMUnsignedInt(), in_object.GetFormat());
        out.SetProperty("type", FBOMUnsignedInt(), in_object.GetType());
        out.SetProperty("filter_mode", FBOMUnsignedInt(), in_object.GetFilterMode());
        out.SetProperty("wrap_mode", FBOMUnsignedInt(), in_object.GetWrapMode());

        const auto num_bytes = in_object.GetImage()->GetByteSize();

        out.SetProperty("num_bytes", FBOMUnsignedLong(), num_bytes);

        if (in_object.GetImage() && in_object.GetImage()->HasAssignedImageData()) {
            const StreamedData *streamed_data = in_object.GetImage()->GetStreamedData();
            const ByteBuffer byte_buffer = streamed_data->Load();

            AssertThrowMsg(byte_buffer.Size() == num_bytes, "num_bytes != byte_buffer.Size()!");
            
            out.SetProperty("bytes", FBOMByteBuffer(byte_buffer.Size()), byte_buffer);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        Extent3D extent;

        {
            if (auto err = in.GetProperty("extent.width").ReadUnsignedInt(&extent.width)) {
                return err;
            }

            if (auto err = in.GetProperty("extent.height").ReadUnsignedInt(&extent.height)) {
                return err;
            }

            if (auto err = in.GetProperty("extent.depth").ReadUnsignedInt(&extent.depth)) {
                return err;
            }
        }

        InternalFormat format = InternalFormat::RGBA8;
        in.GetProperty("format").ReadUnsignedInt(&format);

        ImageType type = ImageType::TEXTURE_TYPE_2D;
        in.GetProperty("type").ReadUnsignedInt(&type);

        FilterMode filter_mode = FilterMode::TEXTURE_FILTER_NEAREST;
        in.GetProperty("filter_mode").ReadUnsignedInt(&filter_mode);

        WrapMode wrap_mode = WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE;
        in.GetProperty("wrap_mode").ReadUnsignedInt(&wrap_mode);

        uint64 num_bytes = 0;
        if (auto err = in.GetProperty("num_bytes").ReadUnsignedLong(&num_bytes)) {
            return err;
        }

        ByteBuffer byte_buffer;

        if (num_bytes != 0) {
            if (auto err = in.GetProperty("bytes").ReadBytes(num_bytes, byte_buffer)) {
                return err;
            }
        }

        out_object = UniquePtr<Handle<Texture>>::Construct(CreateObject<Texture>(
            extent,
            format,
            type,
            filter_mode,
            wrap_mode,
            UniquePtr<MemoryStreamedData>::Construct(std::move(byte_buffer))
        ));

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif
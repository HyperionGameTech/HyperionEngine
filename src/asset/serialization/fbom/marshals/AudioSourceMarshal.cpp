/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <core/object/HypData.hpp>

#include <audio/AudioSource.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<AudioSource> : public FBOMObjectMarshalerBase<AudioSource>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const AudioSource &in_object, FBOMObject &out) const override
    {
        out.SetProperty(NAME("format"), FBOMData::FromUInt32(uint32(in_object.GetFormat())));
        out.SetProperty(NAME("byte_buffer"), FBOMData::FromByteBuffer(in_object.GetByteBuffer()));
        out.SetProperty(NAME("freq"), FBOMData::FromUInt64(in_object.GetFreq()));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        ByteBuffer byte_buffer;

        if (FBOMResult err = in.GetProperty("byte_buffer").ReadByteBuffer(byte_buffer)) {
            return err;
        }

        uint format;

        if (FBOMResult err = in.GetProperty("format").ReadUInt32(&format)) {
            return err;
        }

        SizeType freq;

        if (FBOMResult err = in.GetProperty("freq").ReadUInt64(&freq)) {
            return err;
        }

        out = HypData(CreateObject<AudioSource>(
            AudioSource::Format(format),
            byte_buffer,
            freq
        ));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(AudioSource, FBOMMarshaler<AudioSource>);

} // namespace hyperion::fbom
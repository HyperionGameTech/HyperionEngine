#ifndef HYPERION_V2_FBOM_MARSHALS_SCRIPT_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SCRIPT_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <script/Script.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Script> : public FBOMObjectMarshalerBase<Script>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(Script::GetClass().GetName());
    }

    virtual FBOMResult Serialize(const Script &in_object, FBOMObject &out) const override
    {
        out.SetProperty("src_data", in_object.GetSourceFile().GetBuffer());
        out.SetProperty("src_filepath", FBOMString(), in_object.GetSourceFile().GetFilePath().Size(), in_object.GetSourceFile().GetFilePath().Data());

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        ByteBuffer src_bytes;

        if (auto err = in.GetProperty("src_data").ReadByteBuffer(src_bytes)) {
            return err;
        }

        String filepath;

        if (auto err = in.GetProperty("src_filepath").ReadString(filepath)) {
            return err;
        }

        SourceFile source_file(filepath.Data(), src_bytes.Size());
        source_file.ReadIntoBuffer(src_bytes.Data(), src_bytes.Size());

        out_object = UniquePtr<Handle<Script>>::Construct(CreateObject<Script>(source_file));

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif
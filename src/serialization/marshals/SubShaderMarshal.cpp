/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>

#include <core/object/HypData.hpp>

#include <rendering/Shader.hpp>

namespace hyperion::fbom {

#if 0

template <>
class FBOMMarshaler<SubShader> : public FBOMObjectMarshalerBase<SubShader>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const SubShader &in_object, FBOMObject &out) const override
    {
        out.SetProperty("Type", FBOMData::FromUInt32(in_object.type));
        out.SetProperty("Bytes", FBOMData::FromByteBuffer(in_object.spirv.bytes, FBOMDataFlags::COMPRESSED));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(fbom::FBOMLoadContext &context, const FBOMObject &in, HypData &out) const override
    {
        SubShader sub_shader;

        if (FBOMResult err = in.GetProperty("Type").ReadUInt32(&sub_shader.type)) {
            return err;
        }

        if (const FBOMData &bytes_property = in.GetProperty("Bytes")) {
            if (FBOMResult err = bytes_property.ReadByteBuffer(sub_shader.spirv.bytes)) {
                return err;
            }
        }

        out = HypData(std::move(sub_shader));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(SubShader, FBOMMarshaler<SubShader>);

#endif

} // namespace hyperion::fbom
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>

#include <rendering/Shader.hpp>

#include <core/object/HypData.hpp>

#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<SubShader> : public FBOMObjectMarshalerBase<SubShader>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const SubShader &in_object, FBOMObject &out) const override
    {
        out.SetProperty("Type", FBOMData::FromUInt32(in_object.type));
        out.SetProperty("Bytes", FBOMSequence(FBOMUInt8(), in_object.spirv.bytes.Size()), in_object.spirv.bytes.Data());

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, HypData &out) const override
    {
        SubShader sub_shader;

        if (FBOMResult err = in.GetProperty("Type").ReadUInt32(&sub_shader.type)) {
            return err;
        }

        if (const FBOMData &bytes_property = in.GetProperty("Bytes")) {
            const SizeType num_bytes = bytes_property.NumElements(FBOMUInt8());

            if (num_bytes != 0) {
                if (FBOMResult err = bytes_property.ReadBytes(num_bytes, sub_shader.spirv.bytes)) {
                    return err;
                }
            }
        }

        out = HypData(std::move(sub_shader));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(SubShader, FBOMMarshaler<SubShader>);

} // namespace hyperion::fbom
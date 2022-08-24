#ifndef HYPERION_V2_FBOM_MARSHALS_SHADER_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SHADER_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/SubShaderMarshal.hpp>
#include <rendering/Shader.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<Shader> : public FBOMObjectMarshalerBase<Shader>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("Shader");
    }

    virtual FBOMResult Serialize(const Shader &in_object, FBOMObject &out) const override
    {
        for (auto &sub_shader : in_object.GetSubShaders()) {
            out.AddChild(sub_shader);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(Engine *, const FBOMObject &in, Shader *&out_object) const override
    {
        std::vector<SubShader> sub_shaders;

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("SubShader")) {
                sub_shaders.push_back(node.deserialized.Get<SubShader>());
            }
        }

        out_object = new Shader(sub_shaders);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif
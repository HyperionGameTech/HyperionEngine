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
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("ShaderInstance");
    }

    virtual FBOMResult Serialize(const Shader &in_object, FBOMObject &out) const override
    {
        out.SetProperty("name", FBOMName(), in_object.GetCompiledShader().GetDefinition().name);

        const VertexAttributeSet required_vertex_attributes = in_object.GetCompiledShader().GetDefinition().properties.GetRequiredVertexAttributes();
        out.SetProperty("required_vertex_attributes", FBOMData::FromUnsignedLong(required_vertex_attributes.flag_mask));

        const VertexAttributeSet optional_vertex_attributes = in_object.GetCompiledShader().GetDefinition().properties.GetOptionalVertexAttributes();
        out.SetProperty("optional_vertex_attributes", FBOMData::FromUnsignedLong(optional_vertex_attributes.flag_mask));

        Array<ShaderProperty> properties_array = in_object.GetCompiledShader().GetDefinition().properties.GetPropertySet().ToArray();

        out.SetProperty("properties.size", FBOMUnsignedInt(), uint(properties_array.Size()));

        for (SizeType index = 0; index < properties_array.Size(); index++) {
            const ShaderProperty &item = properties_array[index];

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".name",
                FBOMString(),
                item.name.Size(),
                item.name.Data()
            );
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        Name name;

        if (auto err = in.GetProperty("name").ReadName(&name)) {
            return err;
        }

        VertexAttributeSet required_vertex_attributes;
        in.GetProperty("required_vertex_attributes").ReadUnsignedLong(&required_vertex_attributes.flag_mask);

        VertexAttributeSet optional_vertex_attributes;
        in.GetProperty("optional_vertex_attributes").ReadUnsignedLong(&optional_vertex_attributes.flag_mask);

        ShaderProperties properties;
        properties.SetRequiredVertexAttributes(required_vertex_attributes);
        properties.SetOptionalVertexAttributes(optional_vertex_attributes);

        uint num_properties;

        if (auto err = in.GetProperty("properties.size").ReadUnsignedInt(&num_properties)) {
            return err;
        }

        for (uint i = 0; i < num_properties; i++) {
            const auto param_string = String("properties.") + String::ToString(i);

            String property_name;

            if (auto err = in.GetProperty(param_string + ".name").ReadString(property_name)) {
                continue;
            }

            properties.Set(property_name);
        }

        Handle<Shader> shader = g_shader_manager->GetOrCreate(name, properties);

        if (!shader.IsValid()) {
            DebugLog(
                LogType::Error,
                "Failed to deserialize Shader instance: The referenced compiled shader is not valid.\n"
                "\tNameID: %llu\n"
                "\tProperties: %s\n",
                name.GetHashCode().Value(),
                properties.ToString().Data()
            );

            return { FBOMResult::FBOM_ERR, "Invalid compiled shader" };
        }

        out_object = UniquePtr<Handle<Shader>>::Construct(std::move(shader));

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif
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
        return FBOMObjectType("ShaderInstance");
    }

    virtual FBOMResult Serialize(const Shader &in_object, FBOMObject &out) const override
    {
        out.SetProperty("name", FBOMName(), in_object.GetCompiledShader().GetDefinition().name);

        Array<ShaderProperty> properties_array = in_object.GetCompiledShader().GetDefinition().properties.ToArray();

        out.SetProperty("properties.size", FBOMUnsignedInt(), UInt(properties_array.Size()));

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

        ShaderProps properties;

        UInt num_properties;

        if (auto err = in.GetProperty("properties.size").ReadUInt32(&num_properties)) {
            return err;
        }

        for (UInt i = 0; i < num_properties; i++) {
            const auto param_string = String("properties.") + String::ToString(i);

            String property_name;

            if (auto err = in.GetProperty(param_string + ".name").ReadString(property_name)) {
                continue;
            }

            properties.Set(property_name);
        }

        Handle<Shader> shader = Engine::Get()->GetShaderManager().GetOrCreate(name, properties);

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
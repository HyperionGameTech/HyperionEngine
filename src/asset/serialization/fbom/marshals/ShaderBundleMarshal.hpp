#ifndef HYPERION_V2_FBOM_MARSHALS_SHADER_BUNDLE_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SHADER_BUNDLE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <util/shader_compiler/ShaderCompiler.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<CompiledShader> : public FBOMObjectMarshalerBase<CompiledShader>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const CompiledShader &in_object, FBOMObject &out) const override
    {
        if (!in_object.IsValid()) {
            return { FBOMResult::FBOM_ERR, "Cannot serialize invalid compiled shader instance" };
        }

        // Set global descriptor table version - if this hashcode changes, the shader is invalid and must be recompiled
        out.SetProperty("global_descriptor_table_version", FBOMUnsignedLong(), renderer::g_static_descriptor_table_decl->GetHashCode().Value());

        out.SetProperty("name", FBOMName(), in_object.GetDefinition().name);

        out.SetProperty("entry_point_name", FBOMString(in_object.entry_point_name.Size()), in_object.entry_point_name.Data());

        const VertexAttributeSet required_vertex_attributes = in_object.GetDefinition().GetProperties().GetRequiredVertexAttributes();
        out.SetProperty("required_vertex_attributes", FBOMData::FromUnsignedLong(required_vertex_attributes.flag_mask));

        const VertexAttributeSet optional_vertex_attributes = in_object.GetDefinition().GetProperties().GetOptionalVertexAttributes();
        out.SetProperty("optional_vertex_attributes", FBOMData::FromUnsignedLong(optional_vertex_attributes.flag_mask));

        out.SetProperty("num_descriptor_usages", FBOMData::FromUnsignedInt(uint32(in_object.GetDescriptorUsages().Size())));

        for (SizeType index = 0; index < in_object.GetDescriptorUsages().Size(); index++) {
            const DescriptorUsage &item = in_object.GetDescriptorUsages()[index];

            const ANSIString descriptor_name_string(item.descriptor_name.LookupString());
            const ANSIString set_name_string(item.set_name.LookupString());

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".slot",
                FBOMData::FromUnsignedInt(item.slot)
            );

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".descriptor_name",
                FBOMString(descriptor_name_string.Size()),
                descriptor_name_string.Data()
            );

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".set_name",
                FBOMString(set_name_string.Size()),
                set_name_string.Data()
            );

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".flags",
                FBOMData::FromUnsignedInt(item.flags)
            );

            out.SetProperty(
                String("descriptor_usages.") + String::ToString(index) + ".num_params",
                FBOMData::FromUnsignedInt(uint32(item.params.Size()))
            );

            uint param_index = 0;

            for (const auto &it : item.params) {
                out.SetProperty(
                    String("descriptor_usages.") + String::ToString(index) + ".params[" + String::ToString(param_index) + "].key",
                    FBOMData::FromString(it.first)
                );

                out.SetProperty(
                    String("descriptor_usages.") + String::ToString(index) + ".params[" + String::ToString(param_index) + "].value",
                    FBOMData::FromString(it.second)
                );

                param_index++;
            }
        }

        auto properties_array = in_object.GetDefinition().properties.GetPropertySet().ToArray();
        out.SetProperty("properties.size", FBOMData::FromUnsignedInt(uint32(properties_array.Size())));

        for (SizeType index = 0; index < properties_array.Size(); index++) {
            const ShaderProperty &item = properties_array[index];

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".name",
                FBOMData::FromString(item.name)
            );

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".is_permutation",
                FBOMData::FromBool(item.is_permutation)
            );

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".flags",
                FBOMData::FromUnsignedInt(item.flags)
            );

            out.SetProperty(
                String("properties.") + String::ToString(index) + ".is_value_group",
                FBOMData::FromBool(item.IsValueGroup())
            );

            if (item.IsValueGroup()) {
                out.SetProperty(
                    String("properties.") + String::ToString(index) + ".num_possible_values",
                    FBOMData::FromUnsignedInt(uint32(item.possible_values.Size()))
                );

                for (SizeType i = 0; i < item.possible_values.Size(); i++) {
                    out.SetProperty(
                        String("properties.") + String::ToString(index) + ".possible_values[" + String::ToString(i) + "]",
                        FBOMData::FromString(item.possible_values[i])
                    );
                }
            }
        }

        for (SizeType index = 0; index < in_object.modules.Size(); index++) {
            const auto &byte_buffer = in_object.modules[index];

            if (byte_buffer.Size()) {
                out.SetProperty(
                    String("module[") + String::ToString(index) + "]",
                    byte_buffer
                );
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        uint64 global_descriptor_table_version = -1;

        if (auto err = in.GetProperty("global_descriptor_table_version").ReadUnsignedLong(&global_descriptor_table_version)) {
            return err;
        }

        if (global_descriptor_table_version != renderer::g_static_descriptor_table_decl->GetHashCode().Value()) {
            DebugLog(
                LogType::Error,
                "Failed to deserialize Shader instance: The global descriptor table version does not match.\n"
                "\tExpected: %llu\n"
                "\tActual: %llu\n",
                renderer::g_static_descriptor_table_decl->GetHashCode().Value(),
                global_descriptor_table_version
            );

            return { FBOMResult::FBOM_ERR, "Global descriptor table version mismatch" };
        }

        auto compiled_shader = UniquePtr<CompiledShader>::Construct();

        Name name;

        if (auto err = in.GetProperty("name").ReadName(&name)) {
            return err;
        }

        if (in.HasProperty("entry_point_name")) {
            if (auto err = in.GetProperty("entry_point_name").ReadString(compiled_shader->entry_point_name)) {
                return err;
            }
        } else {
            compiled_shader->entry_point_name = "main";
        }

        VertexAttributeSet required_vertex_attributes;
        in.GetProperty("required_vertex_attributes").ReadUnsignedLong(&required_vertex_attributes.flag_mask);

        VertexAttributeSet optional_vertex_attributes;
        in.GetProperty("optional_vertex_attributes").ReadUnsignedLong(&optional_vertex_attributes.flag_mask);

        compiled_shader->GetDefinition().GetProperties().SetRequiredVertexAttributes(required_vertex_attributes);
        compiled_shader->GetDefinition().GetProperties().SetOptionalVertexAttributes(optional_vertex_attributes);

        uint num_descriptor_usages = 0;

        if (in.HasProperty("num_descriptor_usages")) {
            in.GetProperty("num_descriptor_usages").ReadUnsignedInt(&num_descriptor_usages);

            if (num_descriptor_usages != 0) {
                for (uint i = 0; i < num_descriptor_usages; i++) {
                    const auto descriptor_usage_index_string = String("descriptor_usages.") + String::ToString(i);

                    DescriptorUsage usage;

                    ANSIString descriptor_name_string;
                    ANSIString set_name_string;

                    if (auto err = in.GetProperty(descriptor_usage_index_string + ".slot").ReadUnsignedInt(&usage.slot)) {
                        return err;
                    }

                    if (auto err = in.GetProperty(descriptor_usage_index_string + ".descriptor_name").ReadString(descriptor_name_string)) {
                        return err;
                    }

                    if (auto err = in.GetProperty(descriptor_usage_index_string + ".set_name").ReadString(set_name_string)) {
                        return err;
                    }

                    usage.descriptor_name = CreateNameFromDynamicString(descriptor_name_string);
                    usage.set_name = CreateNameFromDynamicString(set_name_string);

                    in.GetProperty(descriptor_usage_index_string + ".flags").ReadUnsignedInt(&usage.flags);

                    uint num_params = 0;
                    in.GetProperty(descriptor_usage_index_string + ".num_params").ReadUnsignedInt(&num_params);

                    for (uint j = 0; j < num_params; j++) {
                        const String param_string = descriptor_usage_index_string + ".params[" + String::ToString(j) + "]";

                        String key;
                        String value;

                        if (auto err = in.GetProperty(param_string + ".key").ReadString(key)) {
                            DebugLog(LogType::Error, "Failed to read key for descriptor usage 'key' parameter %s\n", param_string.Data());
                            
                            return err;
                        }

                        if (auto err = in.GetProperty(param_string + ".value").ReadString(value)) {
                            DebugLog(LogType::Error, "Failed to read value for descriptor usage 'value' parameter %s\n", param_string.Data());
                            return err;
                        }

                        usage.params[key] = value;
                    }

                    compiled_shader->GetDescriptorUsages().Add(std::move(usage));
                }
            }
        }

        uint num_properties;

        if (auto err = in.GetProperty("properties.size").ReadUnsignedInt(&num_properties)) {
            return err;
        }

        for (uint i = 0; i < num_properties; i++) {
            const auto param_string = String("properties.") + String::ToString(i);

            ShaderProperty property;

            if (auto err = in.GetProperty(param_string + ".name").ReadString(property.name)) {
                continue;
            }

            bool is_value_group = false;

            in.GetProperty(param_string + ".is_permutation").ReadBool(&property.is_permutation);
            in.GetProperty(param_string + ".flags").ReadUnsignedInt(&property.flags);
            in.GetProperty(param_string + ".is_value_group").ReadBool(&is_value_group);

            if (is_value_group) {
                uint32 num_possible_values = 0;

                if (auto err = in.GetProperty(param_string + ".num_possible_values").ReadUnsignedInt(&num_possible_values)) {
                    continue;
                }

                for (uint32 i = 0; i < num_possible_values; i++) {
                    String possible_value;

                    if (auto err = in.GetProperty(param_string + ".possible_values[" + String::ToString(i) + "]").ReadString(possible_value)) {
                        continue;
                    }

                    property.possible_values.PushBack(std::move(possible_value));
                }
            }

            compiled_shader->GetDefinition().properties.Set(property);
        }

        for (SizeType index = 0; index < ShaderModuleType::MAX; index++) {
            const auto module_property_name = String("module[") + String::ToString(index) + "]";

            if (const auto &property = in.GetProperty(module_property_name)) {
                if (auto err = property.ReadByteBuffer(compiled_shader->modules[index])) {
                    return err;
                }
            }
        }
        if (!compiled_shader->IsValid()) {
            return { FBOMResult::FBOM_ERR, "Cannot deserialize invalid compiled shader instance" };
        }

        out_object = std::move(compiled_shader);

        return { FBOMResult::FBOM_OK };
    }
};

template <>
class FBOMMarshaler<CompiledShaderBatch> : public FBOMObjectMarshalerBase<CompiledShaderBatch>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const CompiledShaderBatch &in_object, FBOMObject &out) const override
    {
        for (auto &compiled_shader : in_object.compiled_shaders) {
            out.AddChild(compiled_shader);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        auto batch = UniquePtr<CompiledShaderBatch>::Construct();

        for (auto &node : *in.nodes) {
            if (node.GetType().IsOrExtends("CompiledShader")) {
                auto compiled_shader = node.deserialized.Get<CompiledShader>();
                AssertThrow(compiled_shader != nullptr);

                batch->compiled_shaders.PushBack(*compiled_shader);
            }
        }

        out_object = std::move(batch);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif
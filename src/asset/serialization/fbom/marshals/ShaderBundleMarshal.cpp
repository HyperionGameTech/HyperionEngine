/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOM.hpp>
#include <util/shader_compiler/ShaderCompiler.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

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
        out.SetProperty(NAME("global_descriptor_table_version"), FBOMData::FromUnsignedLong(renderer::g_static_descriptor_table_decl->GetHashCode().Value()));

        out.SetProperty(NAME("name"), FBOMData::FromName(in_object.GetDefinition().name));

        out.SetProperty(NAME("entry_point_name"), FBOMString(in_object.entry_point_name.Size()), in_object.entry_point_name.Data());

        const VertexAttributeSet required_vertex_attributes = in_object.GetDefinition().GetProperties().GetRequiredVertexAttributes();
        out.SetProperty(NAME("required_vertex_attributes"), FBOMData::FromUnsignedLong(required_vertex_attributes.flag_mask));

        const VertexAttributeSet optional_vertex_attributes = in_object.GetDefinition().GetProperties().GetOptionalVertexAttributes();
        out.SetProperty(NAME("optional_vertex_attributes"), FBOMData::FromUnsignedLong(optional_vertex_attributes.flag_mask));

        out.SetProperty(NAME("num_descriptor_usages"), FBOMData::FromUnsignedInt(uint32(in_object.GetDescriptorUsages().Size())));

        for (SizeType index = 0; index < in_object.GetDescriptorUsages().Size(); index++) {
            const DescriptorUsage &item = in_object.GetDescriptorUsages()[index];

            const ANSIString descriptor_name_string(*item.descriptor_name);
            const ANSIString set_name_string(*item.set_name);

            out.SetProperty(
                CreateNameFromDynamicString(ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".slot"),
                FBOMData::FromUnsignedInt(item.slot)
            );

            out.SetProperty(
                CreateNameFromDynamicString(ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".descriptor_name"),
                FBOMString(descriptor_name_string.Size()),
                descriptor_name_string.Data()
            );

            out.SetProperty(
                CreateNameFromDynamicString(ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".set_name"),
                FBOMString(set_name_string.Size()),
                set_name_string.Data()
            );

            out.SetProperty(
                CreateNameFromDynamicString(ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".flags"),
                FBOMData::FromUnsignedInt(item.flags)
            );

            out.SetProperty(
                CreateNameFromDynamicString(ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".num_params"),
                FBOMData::FromUnsignedInt(uint32(item.params.Size()))
            );

            uint32 param_index = 0;

            for (const auto &it : item.params) {
                out.SetProperty(
                    CreateNameFromDynamicString(ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".params[" + ANSIString::ToString(param_index) + "].key"),
                    FBOMData::FromString(it.first)
                );

                out.SetProperty(
                    CreateNameFromDynamicString(ANSIString("descriptor_usages.") + ANSIString::ToString(index) + ".params[" + ANSIString::ToString(param_index) + "].value"),
                    FBOMData::FromString(it.second)
                );

                param_index++;
            }
        }

        auto properties_array = in_object.GetDefinition().GetProperties().GetPropertySet().ToArray();
        out.SetProperty(NAME("properties.size"), FBOMData::FromUnsignedInt(uint32(properties_array.Size())));

        for (SizeType index = 0; index < properties_array.Size(); index++) {
            const ShaderProperty &item = properties_array[index];

            out.SetProperty(
                CreateNameFromDynamicString(ANSIString("properties.") + ANSIString::ToString(index) + ".name"),
                FBOMData::FromString(item.name)
            );

            out.SetProperty(
                CreateNameFromDynamicString(ANSIString("properties.") + ANSIString::ToString(index) + ".is_permutation"),
                FBOMData::FromBool(item.is_permutation)
            );

            out.SetProperty(
                CreateNameFromDynamicString(ANSIString("properties.") + ANSIString::ToString(index) + ".flags"),
                FBOMData::FromUnsignedInt(item.flags)
            );

            out.SetProperty(
                CreateNameFromDynamicString(ANSIString("properties.") + ANSIString::ToString(index) + ".is_value_group"),
                FBOMData::FromBool(item.IsValueGroup())
            );

            if (item.IsValueGroup()) {
                out.SetProperty(
                    CreateNameFromDynamicString(ANSIString("properties.") + ANSIString::ToString(index) + ".num_possible_values"),
                    FBOMData::FromUnsignedInt(uint32(item.possible_values.Size()))
                );

                for (SizeType i = 0; i < item.possible_values.Size(); i++) {
                    out.SetProperty(
                        CreateNameFromDynamicString(ANSIString("properties.") + ANSIString::ToString(index) + ".possible_values[" + ANSIString::ToString(i) + "]"),
                        FBOMData::FromString(item.possible_values[i])
                    );
                }
            }
        }

        for (SizeType index = 0; index < in_object.modules.Size(); index++) {
            const ByteBuffer &byte_buffer = in_object.modules[index];

            if (byte_buffer.Size()) {
                out.SetProperty(CreateNameFromDynamicString(ANSIString("module[") + ANSIString::ToString(index) + "]"), byte_buffer);
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        uint64 global_descriptor_table_version = -1;

        if (FBOMResult err = in.GetProperty("global_descriptor_table_version").ReadUnsignedLong(&global_descriptor_table_version)) {
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

        CompiledShader compiled_shader;

        Name name;

        if (FBOMResult err = in.GetProperty("name").ReadName(&name)) {
            return err;
        }

        if (in.HasProperty("entry_point_name")) {
            if (FBOMResult err = in.GetProperty("entry_point_name").ReadString(compiled_shader.entry_point_name)) {
                return err;
            }
        } else {
            compiled_shader.entry_point_name = "main";
        }

        VertexAttributeSet required_vertex_attributes;
        in.GetProperty("required_vertex_attributes").ReadUnsignedLong(&required_vertex_attributes.flag_mask);

        VertexAttributeSet optional_vertex_attributes;
        in.GetProperty("optional_vertex_attributes").ReadUnsignedLong(&optional_vertex_attributes.flag_mask);

        compiled_shader.GetDefinition().GetProperties().SetRequiredVertexAttributes(required_vertex_attributes);
        compiled_shader.GetDefinition().GetProperties().SetOptionalVertexAttributes(optional_vertex_attributes);

        uint32 num_descriptor_usages = 0;

        if (in.HasProperty("num_descriptor_usages")) {
            in.GetProperty("num_descriptor_usages").ReadUnsignedInt(&num_descriptor_usages);

            if (num_descriptor_usages != 0) {
                for (uint32 i = 0; i < num_descriptor_usages; i++) {
                    const ANSIString descriptor_usage_index_string = ANSIString("descriptor_usages.") + ANSIString::ToString(i);

                    DescriptorUsage usage;

                    ANSIString descriptor_name_string;
                    ANSIString set_name_string;

                    if (FBOMResult err = in.GetProperty(descriptor_usage_index_string + ".slot").ReadUnsignedInt(&usage.slot)) {
                        return err;
                    }

                    if (FBOMResult err = in.GetProperty(descriptor_usage_index_string + ".descriptor_name").ReadString(descriptor_name_string)) {
                        return err;
                    }

                    if (FBOMResult err = in.GetProperty(descriptor_usage_index_string + ".set_name").ReadString(set_name_string)) {
                        return err;
                    }

                    usage.descriptor_name = CreateNameFromDynamicString(descriptor_name_string);
                    usage.set_name = CreateNameFromDynamicString(set_name_string);

                    in.GetProperty(descriptor_usage_index_string + ".flags").ReadUnsignedInt(&usage.flags);

                    uint32 num_params = 0;
                    in.GetProperty(descriptor_usage_index_string + ".num_params").ReadUnsignedInt(&num_params);

                    for (uint32 j = 0; j < num_params; j++) {
                        const ANSIString param_string = descriptor_usage_index_string + ".params[" + ANSIString::ToString(j) + "]";

                        String key;
                        String value;

                        if (FBOMResult err = in.GetProperty(param_string + ".key").ReadString(key)) {
                            DebugLog(LogType::Error, "Failed to read key for descriptor usage 'key' parameter %s\n", param_string.Data());
                            
                            return err;
                        }

                        if (FBOMResult err = in.GetProperty(param_string + ".value").ReadString(value)) {
                            DebugLog(LogType::Error, "Failed to read value for descriptor usage 'value' parameter %s\n", param_string.Data());
                            return err;
                        }

                        usage.params[key] = value;
                    }

                    compiled_shader.GetDescriptorUsages().Add(std::move(usage));
                }
            }
        }

        uint32 num_properties;

        if (FBOMResult err = in.GetProperty("properties.size").ReadUnsignedInt(&num_properties)) {
            return err;
        }

        for (uint32 i = 0; i < num_properties; i++) {
            const ANSIString param_string = ANSIString("properties.") + ANSIString::ToString(i);

            ShaderProperty property;

            if (FBOMResult err = in.GetProperty(param_string + ".name").ReadString(property.name)) {
                continue;
            }

            bool is_value_group = false;

            in.GetProperty(param_string + ".is_permutation").ReadBool(&property.is_permutation);
            in.GetProperty(param_string + ".flags").ReadUnsignedInt(&property.flags);
            in.GetProperty(param_string + ".is_value_group").ReadBool(&is_value_group);

            if (is_value_group) {
                uint32 num_possible_values = 0;

                if (FBOMResult err = in.GetProperty(param_string + ".num_possible_values").ReadUnsignedInt(&num_possible_values)) {
                    continue;
                }

                for (uint32 i = 0; i < num_possible_values; i++) {
                    String possible_value;

                    if (FBOMResult err = in.GetProperty(param_string + ".possible_values[" + ANSIString::ToString(i) + "]").ReadString(possible_value)) {
                        continue;
                    }

                    property.possible_values.PushBack(std::move(possible_value));
                }
            }

            compiled_shader.GetDefinition().properties.Set(property);
        }

        for (SizeType index = 0; index < ShaderModuleType::MAX; index++) {
            const ANSIString module_property_name = ANSIString("module[") + ANSIString::ToString(index) + "]";

            if (const FBOMData &property = in.GetProperty(module_property_name)) {
                if (FBOMResult err = property.ReadByteBuffer(compiled_shader.modules[index])) {
                    return err;
                }
            }
        }
        if (!compiled_shader.IsValid()) {
            return { FBOMResult::FBOM_ERR, "Cannot deserialize invalid compiled shader instance" };
        }

        out_object = std::move(compiled_shader);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShader, FBOMMarshaler<CompiledShader>);

template <>
class FBOMMarshaler<CompiledShaderBatch> : public FBOMObjectMarshalerBase<CompiledShaderBatch>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const CompiledShaderBatch &in_object, FBOMObject &out) const override
    {
        for (const CompiledShader &compiled_shader : in_object.compiled_shaders) {
            out.AddChild(compiled_shader);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, Any &out_object) const override
    {
        CompiledShaderBatch batch;

        for (FBOMObject &node : *in.nodes) {
            if (node.GetType().IsOrExtends("CompiledShader")) {
                CompiledShader *compiled_shader = node.deserialized.TryGet<CompiledShader>();

                if (compiled_shader != nullptr) {
                    batch.compiled_shaders.PushBack(*compiled_shader);
                }
            }
        }

        out_object = std::move(batch);

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShaderBatch, FBOMMarshaler<CompiledShaderBatch>);

} // namespace hyperion::fbom
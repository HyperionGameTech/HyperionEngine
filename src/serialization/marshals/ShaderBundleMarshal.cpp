/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypData.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

namespace hyperion {
HYP_DECLARE_LOG_CHANNEL(ShaderCompiler);
} // namespace hyperion

namespace hyperion::serialization {

template <>
class FBOMMarshaler<CompiledShader> : public FBOMObjectMarshalerBase<CompiledShader>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const CompiledShader& in_object, FBOMObject& out) const override
    {
        if (!in_object.IsValid())
        {
            return { FBOMResult::FBOM_ERR, "Cannot serialize invalid compiled shader instance" };
        }

        // Set global descriptor table version - if this hashcode changes, the shader is invalid and must be recompiled
        out.SetProperty("global_descriptor_table_version", FBOMData::FromUInt64(renderer::GetStaticDescriptorTableDeclaration().GetHashCode().Value()));

        out.SetProperty("name", FBOMData::FromName(in_object.definition.name));

        out.SetProperty("EntryPointName", FBOMData::FromString(in_object.entry_point_name));

        const VertexAttributeSet required_vertex_attributes = in_object.definition.properties.GetRequiredVertexAttributes();
        out.SetProperty("required_vertex_attributes", FBOMData::FromUInt64(required_vertex_attributes.flag_mask));

        const VertexAttributeSet optional_vertex_attributes = in_object.definition.properties.GetOptionalVertexAttributes();
        out.SetProperty("optional_vertex_attributes", FBOMData::FromUInt64(optional_vertex_attributes.flag_mask));

        for (const DescriptorUsage& descriptor_usage : in_object.descriptor_usage_set.elements)
        {
            out.AddChild(descriptor_usage);
        }

        Array<ShaderProperty> properties_array = in_object.definition.properties.GetPropertySet().ToArray();
        out.SetProperty("properties.size", FBOMData::FromUInt32(uint32(properties_array.Size())));

        for (SizeType index = 0; index < properties_array.Size(); index++)
        {
            const ShaderProperty& item = properties_array[index];

            // @TODO: Serialize `value`

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".name",
                FBOMData::FromString(item.name));

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".is_permutation",
                FBOMData::FromBool(item.is_permutation));

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".flags",
                FBOMData::FromUInt32(item.flags));

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".is_value_group",
                FBOMData::FromBool(item.IsValueGroup()));

            if (item.IsValueGroup())
            {
                out.SetProperty(
                    ANSIString("properties.") + ANSIString::ToString(index) + ".num_possible_values",
                    FBOMData::FromUInt32(uint32(item.possible_values.Size())));

                for (SizeType i = 0; i < item.possible_values.Size(); i++)
                {
                    out.SetProperty(
                        ANSIString("properties.") + ANSIString::ToString(index) + ".possible_values[" + ANSIString::ToString(i) + "]",
                        FBOMData::FromString(item.possible_values[i]));
                }
            }
        }

        for (SizeType index = 0; index < in_object.modules.Size(); index++)
        {
            const ByteBuffer& byte_buffer = in_object.modules[index];

            if (byte_buffer.Size())
            {
                out.SetProperty(ANSIString("module[") + ANSIString::ToString(index) + "]", FBOMData::FromByteBuffer(byte_buffer));
            }
        }

        // HYP_LOG(Serialization, Info, "Serialized shader '{}' with properties:", in_object.definition.name);

        // String properties_string;

        // for (const ShaderProperty &property : properties_array) {
        //     properties_string += "\t" + property.name + "\n";
        // }

        // HYP_LOG(Serialization, Info, "\t{}", properties_string);

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        uint64 global_descriptor_table_version = -1;

        if (FBOMResult err = in.GetProperty("global_descriptor_table_version").ReadUInt64(&global_descriptor_table_version))
        {
            return err;
        }

        if (global_descriptor_table_version != renderer::GetStaticDescriptorTableDeclaration().GetHashCode().Value())
        {
            HYP_LOG(ShaderCompiler, Info, "The global descriptor table version does not match. This shader will need to be recompiled.");

            return { FBOMResult::FBOM_ERR, "Global descriptor table version mismatch" };
        }

        CompiledShader compiled_shader;

        if (FBOMResult err = in.GetProperty("name").ReadName(&compiled_shader.definition.name))
        {
            return err;
        }

        if (in.HasProperty("EntryPointName"))
        {
            if (FBOMResult err = in.GetProperty("EntryPointName").ReadString(compiled_shader.entry_point_name))
            {
                return err;
            }
        }
        else
        {
            compiled_shader.entry_point_name = "main";
        }

        VertexAttributeSet required_vertex_attributes {};

        if (FBOMResult err = in.GetProperty("required_vertex_attributes").ReadUInt64(&required_vertex_attributes.flag_mask))
        {
            return err;
        }

        compiled_shader.definition.properties.SetRequiredVertexAttributes(required_vertex_attributes);

        VertexAttributeSet optional_vertex_attributes {};

        if (FBOMResult err = in.GetProperty("optional_vertex_attributes").ReadUInt64(&optional_vertex_attributes.flag_mask))
        {
            return err;
        }

        compiled_shader.definition.properties.SetOptionalVertexAttributes(optional_vertex_attributes);

        uint32 num_properties;

        if (FBOMResult err = in.GetProperty("properties.size").ReadUInt32(&num_properties))
        {
            return err;
        }

        for (uint32 i = 0; i < num_properties; i++)
        {
            const ANSIString param_string = ANSIString("properties.") + ANSIString::ToString(i);

            ShaderProperty property;

            if (FBOMResult err = in.GetProperty(param_string + ".name").ReadString(property.name))
            {
                continue;
            }

            bool is_value_group = false;

            in.GetProperty(param_string + ".is_permutation").ReadBool(&property.is_permutation);
            in.GetProperty(param_string + ".flags").ReadUInt32(&property.flags);
            in.GetProperty(param_string + ".is_value_group").ReadBool(&is_value_group);

            if (is_value_group)
            {
                uint32 num_possible_values = 0;

                if (FBOMResult err = in.GetProperty(param_string + ".num_possible_values").ReadUInt32(&num_possible_values))
                {
                    continue;
                }

                for (uint32 i = 0; i < num_possible_values; i++)
                {
                    String possible_value;

                    if (FBOMResult err = in.GetProperty(param_string + ".possible_values[" + ANSIString::ToString(i) + "]").ReadString(possible_value))
                    {
                        continue;
                    }

                    property.possible_values.PushBack(std::move(possible_value));
                }
            }

            compiled_shader.definition.properties.Set(property);
        }

        for (SizeType index = 0; index < ShaderModuleType::MAX; index++)
        {
            const ANSIString module_property_name = ANSIString("module[") + ANSIString::ToString(index) + "]";

            if (const FBOMData& property = in.GetProperty(module_property_name))
            {
                if (FBOMResult err = property.ReadByteBuffer(compiled_shader.modules[index]))
                {
                    return err;
                }
            }
        }

        for (const FBOMObject& child : in.GetChildren())
        {
            if (child.GetType().GetNativeTypeID() == TypeID::ForType<DescriptorUsage>())
            {
                compiled_shader.descriptor_usage_set.Add(child.m_deserialized_object->Get<DescriptorUsage>());
            }
        }

        compiled_shader.descriptor_table_declaration = compiled_shader.descriptor_usage_set.BuildDescriptorTableDeclaration();

        out = HypData(std::move(compiled_shader));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShader, FBOMMarshaler<CompiledShader>);

template <>
class FBOMMarshaler<CompiledShaderBatch> : public FBOMObjectMarshalerBase<CompiledShaderBatch>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const CompiledShaderBatch& in_object, FBOMObject& out) const override
    {
        for (const CompiledShader& compiled_shader : in_object.compiled_shaders)
        {
            out.AddChild(compiled_shader);
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        CompiledShaderBatch batch;

        for (const FBOMObject& child : in.GetChildren())
        {
            if (child.GetType().IsOrExtends("CompiledShader"))
            {
                Optional<CompiledShader&> compiled_shader_opt = child.m_deserialized_object->TryGet<CompiledShader>();

                if (compiled_shader_opt.HasValue())
                {
                    batch.compiled_shaders.PushBack(*compiled_shader_opt);
                }
                else
                {
                    HYP_LOG(Serialization, Error, "Failed to deserialize CompiledShader instance");
                }
            }
        }

        out = HypData(std::move(batch));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShaderBatch, FBOMMarshaler<CompiledShaderBatch>);

} // namespace hyperion::serialization
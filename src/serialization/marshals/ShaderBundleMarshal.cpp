/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/reflection/HypData.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

namespace hyperion {
HYP_DECLARE_LOG_CHANNEL(ShaderCompiler);
} // namespace hyperion

namespace hyperion::serialization {

class CompiledShaderMarshal : public HypClassInstanceMarshal
{
public:
    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        return HypClassInstanceMarshal::Serialize(in, out);
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        CompiledShader compiledShader;

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, CompiledShader::Class(), out))
        {
            return err;
        }

        if (compiledShader.GetRevisionNumber() != GetStaticDescriptorTableDeclaration().GetHashCode().Value())
        {
            // force recompile
            return { FBOMResult::FBOM_ERR, "Shader out of date" };
        }

        out = HypData(std::move(compiledShader));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShader, CompiledShaderMarshal);

/*
class CompiledShaderMarshal : public FBOMObjectMarshalerBase<CompiledShader>
{
public:
    virtual FBOMResult Serialize(const CompiledShader& inObject, FBOMObject& out) const override
    {
        if (!inObject.IsValid())
        {
            return { FBOMResult::FBOM_ERR, "Cannot serialize invalid compiled shader instance" };
        }

        // Set global descriptor table version - if this hashcode changes, the shader is invalid and must be recompiled
        out.SetProperty("global_descriptor_table_version", FBOMData::FromUInt64(GetStaticDescriptorTableDeclaration().GetHashCode().Value()));

        out.SetProperty("name", FBOMData::FromString(String(*inObject.definition.name)));

        out.SetProperty("EntryPointName", FBOMData::FromString(inObject.entryPointName));

        const VertexAttributeSet requiredVertexAttributes = inObject.definition.properties.GetRequiredVertexAttributes();
        out.SetProperty("required_vertex_attributes", FBOMData::FromUInt64(requiredVertexAttributes.flagMask));

        const VertexAttributeSet optionalVertexAttributes = inObject.definition.properties.GetOptionalVertexAttributes();
        out.SetProperty("optional_vertex_attributes", FBOMData::FromUInt64(optionalVertexAttributes.flagMask));

        for (const DescriptorUsage& descriptorUsage : inObject.descriptorUsageSet.elements)
        {
            out.AddChild(descriptorUsage);
        }

        Array<ShaderProperty> propertiesArray = inObject.definition.properties.GetPropertySet().ToArray();
        out.SetProperty("properties.size", FBOMData::FromUInt32(uint32(propertiesArray.Size())));

        for (SizeType index = 0; index < propertiesArray.Size(); index++)
        {
            const ShaderProperty& item = propertiesArray[index];

            // @TODO: Serialize `value`

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".name",
                FBOMData::FromString(String(item.name.LookupString())));

            out.SetProperty(
                ANSIString("properties.") + ANSIString::ToString(index) + ".is_permutation",
                FBOMData::FromBool(item.isPermutation));

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
                    FBOMData::FromUInt32(uint32(item.enumValues.Size())));

                for (SizeType i = 0; i < item.enumValues.Size(); i++)
                {
                    FBOMData enumValueData;
                    if (FBOMResult err = HypData(item.enumValues[i]).Serialize(enumValueData))
                    {
                        HYP_BREAKPOINT;
                        return err;
                    }

                    out.SetProperty(
                        ANSIString("properties.") + ANSIString::ToString(index) + ".possible_values[" + ANSIString::ToString(i) + "]",
                        std::move(enumValueData));
                }
            }
        }

        for (SizeType index = 0; index < inObject.modules.Size(); index++)
        {
            const ByteBuffer& byteBuffer = inObject.modules[index];

            if (byteBuffer.Size())
            {
                out.SetProperty(ANSIString("module[") + ANSIString::ToString(index) + "]", FBOMData::FromByteBuffer(byteBuffer));
            }
        }

        // HYP_LOG(Serialization, Info, "Serialized shader '{}' with properties:", inObject.definition.name);

        // String propertiesString;

        // for (const ShaderProperty &property : propertiesArray) {
        //     propertiesString += "\t" + property.name + "\n";
        // }

        // HYP_LOG(Serialization, Info, "\t{}", propertiesString);

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        uint64 globalDescriptorTableVersion = -1;

        if (FBOMResult err = in.GetProperty("global_descriptor_table_version").ReadUInt64(&globalDescriptorTableVersion))
        {
            return err;
        }

        if (globalDescriptorTableVersion != GetStaticDescriptorTableDeclaration().GetHashCode().Value())
        {
            return { FBOMResult::FBOM_ERR, "Shader out of date" };
        }

        CompiledShader compiledShader;

        String strName;

        if (FBOMResult err = in.GetProperty("name").ReadString(strName))
        {
            return err;
        }

        compiledShader.definition.name = CreateNameFromDynamicString(*strName);

        if (in.HasProperty("EntryPointName"))
        {
            if (FBOMResult err = in.GetProperty("EntryPointName").ReadString(compiledShader.entryPointName))
            {
                return err;
            }
        }
        else
        {
            compiledShader.entryPointName = "main";
        }

        VertexAttributeSet requiredVertexAttributes {};

        if (FBOMResult err = in.GetProperty("required_vertex_attributes").ReadUInt64(&requiredVertexAttributes.flagMask))
        {
            return err;
        }

        compiledShader.definition.properties.SetRequiredVertexAttributes(requiredVertexAttributes);

        VertexAttributeSet optionalVertexAttributes {};

        if (FBOMResult err = in.GetProperty("optional_vertex_attributes").ReadUInt64(&optionalVertexAttributes.flagMask))
        {
            return err;
        }

        compiledShader.definition.properties.SetOptionalVertexAttributes(optionalVertexAttributes);

        uint32 numProperties;

        if (FBOMResult err = in.GetProperty("properties.size").ReadUInt32(&numProperties))
        {
            return err;
        }

        for (uint32 i = 0; i < numProperties; i++)
        {
            const ANSIString paramString = ANSIString("properties.") + ANSIString::ToString(i);

            ShaderProperty property;

            String strName;
            if (FBOMResult err = in.GetProperty(paramString + ".name").ReadString(strName))
            {
                return err;
            }

            property.name = CreateNameFromDynamicString(*strName);

            bool isValueGroup = false;

            in.GetProperty(paramString + ".is_permutation").ReadBool(&property.isPermutation);
            in.GetProperty(paramString + ".flags").ReadUInt32(&property.flags);
            in.GetProperty(paramString + ".is_value_group").ReadBool(&isValueGroup);

            if (isValueGroup)
            {
                uint32 numEnumValues = 0;

                if (FBOMResult err = in.GetProperty(paramString + ".num_possible_values").ReadUInt32(&numEnumValues))
                {
                    return err;
                }

                for (uint32 i = 0; i < numEnumValues; i++)
                {
                    ShaderProperty::Value enumValue;

                    if (FBOMResult err = HypData::Deserialize<ShaderProperty::Value>(context, in.GetProperty(paramString + ".possible_values[" + ANSIString::ToString(i) + "]"), enumValue))
                    {
                        HYP_BREAKPOINT;
                        return err;
                    }

                    property.enumValues.PushBack(std::move(enumValue));
                }
            }

            compiledShader.definition.properties.Set(property);
        }

        for (SizeType index = 0; index < SMT_MAX; index++)
        {
            const ANSIString modulePropertyName = ANSIString("module[") + ANSIString::ToString(index) + "]";

            if (const FBOMData& property = in.GetProperty(modulePropertyName))
            {
                if (FBOMResult err = property.ReadByteBuffer(compiledShader.modules[index]))
                {
                    return err;
                }
            }
        }

        for (const FBOMObject& child : in.GetChildren())
        {
            if (child.GetType().GetNativeTypeId() == TypeId::ForType<DescriptorUsage>())
            {
                compiledShader.descriptorUsageSet.Add(child.m_deserializedObject->Get<DescriptorUsage>());
            }
        }

        compiledShader.descriptorTableDeclaration = compiledShader.descriptorUsageSet.BuildDescriptorTableDeclaration();

        out = HypData(std::move(compiledShader));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(CompiledShader, CompiledShaderMarshal);

class CompiledShaderBatchMarshal : public FBOMObjectMarshalerBase<CompiledShaderBatch>
{
public:
    virtual FBOMResult Serialize(const CompiledShaderBatch& inObject, FBOMObject& out) const override
    {
        for (const CompiledShader& compiledShader : inObject.compiledShaders)
        {
            out.AddChild(compiledShader);
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
                Optional<CompiledShader&> compiledShaderOpt = child.m_deserializedObject->TryGet<CompiledShader>();

                if (compiledShaderOpt.HasValue())
                {
                    batch.compiledShaders.PushBack(*compiledShaderOpt);
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

HYP_DEFINE_MARSHAL(CompiledShaderBatch, CompiledShaderBatchMarshal);*/

} // namespace hyperion::serialization
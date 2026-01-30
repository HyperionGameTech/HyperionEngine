#pragma once

#include <rendering/util/ShaderCompiler.hpp>

namespace Hyperion {

namespace ShaderCompilerUtil {

namespace DXIL {

// static String GetTypeName(const D3D12_SHADER_TYPE_DESC& typeDesc)
// {
//     String typeName;
    
//     switch (typeDesc.Type)
//     {
//     case D3D_SVT_BOOL:      typeName = "bool"; break;
//     case D3D_SVT_INT:       typeName = "int"; break;
//     case D3D_SVT_UINT:      typeName = "uint"; break;
//     case D3D_SVT_FLOAT:     typeName = "float"; break;
//     case D3D_SVT_DOUBLE:    typeName = "double"; break;
//     case D3D_SVT_MIN16FLOAT: typeName = "min16float"; break;
//     case D3D_SVT_MIN16INT:  typeName = "min16int"; break;
//     case D3D_SVT_MIN16UINT: typeName = "min16uint"; break;
//     default:                typeName = "unknown"; break;
//     }

//     // Handle vectors
//     if (typeDesc.Class == D3D_SVC_VECTOR)
//     {
//         typeName += String::ToString(typeDesc.Columns);
//     }
//     // Handle matrices
//     else if (typeDesc.Class == D3D_SVC_MATRIX_ROWS || typeDesc.Class == D3D_SVC_MATRIX_COLUMNS)
//     {
//         typeName += String::ToString(typeDesc.Rows) + "x" + String::ToString(typeDesc.Columns);
//     }

//     return typeName;
// }

// static String GetFieldTypeName(const D3D12_SHADER_TYPE_DESC& typeDesc)
// {
//     String fieldTypeName;

//     if (typeDesc.Class == D3D_SVC_STRUCT)
//     {
//         if (typeDesc.Name && typeDesc.Name[0] != '\0')
//         {
//             fieldTypeName = typeDesc.Name;
//         }
//         else
//         {
//             fieldTypeName = "struct";
//         }
//     }
//     else
//     {
//         fieldTypeName = GetTypeName(typeDesc);

//         if (typeDesc.Elements > 0)
//         {
//             fieldTypeName += "[" + String::ToString(typeDesc.Elements) + "]";
//         }
//     }

//     return fieldTypeName;
// }

static void HandleShaderStruct(ID3D12ShaderReflectionType* pType, ShaderStruct& outStructureType)
{
}

static void ReflectConstantBuffer(ID3D12ShaderReflection* pReflection, const D3D12_SHADER_INPUT_BIND_DESC& bindDesc, ShaderInput& shaderInput)
{
}

static void ReflectStructuredBuffer(ID3D12ShaderReflection* pReflection, const D3D12_SHADER_INPUT_BIND_DESC& bindDesc, ShaderInput& shaderInput)
{
}

static void ReflectDXIL(ID3D12ShaderReflection* pReflection, ShaderInputGroup& inputGroup, Array<String>& outErrorMessages)
{
    HYP_NOT_IMPLEMENTED();
}

} // namespace DXIL

static void ReflectDXIL(ID3D12ShaderReflection* pReflection, ShaderInputGroup& inputGroup, Array<String>& outErrorMessages)
{
    DXIL::ReflectDXIL(pReflection, inputGroup, outErrorMessages);
}

#if HYP_SPIRV_REFLECT

namespace SPIRV {

static String GetTypeName(const SpvReflectTypeDescription* typeDesc)
{
    if (!typeDesc)
    {
        return "unknown";
    }

    String typeName;
    
    if (typeDesc->type_flags & SPV_REFLECT_TYPE_FLAG_BOOL)
    {
        typeName = "bool";
    }
    else if (typeDesc->type_flags & SPV_REFLECT_TYPE_FLAG_INT)
    {
        if (typeDesc->traits.numeric.scalar.signedness)
        {
            typeName = "int";
        }
        else
        {
            typeName = "uint";
        }
    }
    else if (typeDesc->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
    {
        if (typeDesc->traits.numeric.scalar.width == 64)
        {
            typeName = "double";
        }
        else
        {
            typeName = "float";
        }
    }
    else if (typeDesc->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT)
    {
        typeName = typeDesc->type_name ? typeDesc->type_name : "struct";
    }
    else
    {
        typeName = "unknown";
    }

    // Handle vectors
    if (typeDesc->type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR)
    {
        typeName += String::ToString(typeDesc->traits.numeric.vector.component_count);
    }
    // Handle matrices
    else if (typeDesc->type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
    {
        typeName += String::ToString(typeDesc->traits.numeric.matrix.row_count) 
            + "x" + String::ToString(typeDesc->traits.numeric.matrix.column_count);
    }

    return typeName;
}

static String GetFieldTypeName(const SpvReflectTypeDescription* typeDesc)
{
    if (!typeDesc)
    {
        return "unknown";
    }

    String fieldTypeName;

    if (typeDesc->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT)
    {
        if (typeDesc->type_name && typeDesc->type_name[0] != '\0')
        {
            fieldTypeName = typeDesc->type_name;
        }
        else
        {
            fieldTypeName = "struct";
        }
    }
    else
    {
        fieldTypeName = GetTypeName(typeDesc);

        if (typeDesc->type_flags & SPV_REFLECT_TYPE_FLAG_ARRAY)
        {
            for (uint32 i = 0; i < typeDesc->traits.array.dims_count; i++)
            {
                fieldTypeName += "[" + String::ToString(typeDesc->traits.array.dims[i]) + "]";
            }
        }
    }

    return fieldTypeName;
}

static void HandleShaderStruct(const SpvReflectBlockVariable* pBlock, ShaderStruct& inOutStruct)
{
    if (!pBlock)
    {
        return;
    }

    for (uint32 memberIndex = 0; memberIndex < pBlock->member_count; memberIndex++)
    {
        const SpvReflectBlockVariable& member = pBlock->members[memberIndex];

        if (!member.name)
        {
            continue;
        }

        const String fieldTypeName = GetFieldTypeName(member.type_description);

        auto& field = inOutStruct.AddField(
            CreateNameFromDynamicString(member.name),
            ShaderStruct(CreateNameFromDynamicString(fieldTypeName), member.size)).second;

        if (member.member_count > 0)
        {
            HandleShaderStruct(&member, field);
        }
    }
}

static void ReflectConstantBuffer(const SpvReflectDescriptorBinding* binding, ShaderInput& shaderInput)
{
    if (!binding)
    {
        return;
    }

    shaderInput.structInfo.size = binding->block.size;
    shaderInput.structInfo.name = CreateNameFromDynamicString(binding->name);

    HandleShaderStruct(&binding->block, shaderInput.structInfo);
}

static void ReflectStructuredBuffer(const SpvReflectDescriptorBinding* binding, ShaderInput& shaderInput)
{
    if (!binding)
    {
        return;
    }

    shaderInput.structInfo.size = (SizeType)binding->block.size;
    shaderInput.structInfo.name = CreateNameFromDynamicString(binding->name);

    HandleShaderStruct(&binding->block, shaderInput.structInfo);
}

static void ReflectSPIRV(SpvReflectShaderModule* pModule, ShaderInputGroup& inputGroup, Array<String>& outErrorMessages)
{
    uint32 bindingCount = 0;
    spvReflectEnumerateDescriptorBindings(pModule, &bindingCount, nullptr);

    if (bindingCount == 0)
    {
        return;
    }

    Array<SpvReflectDescriptorBinding*> allBindings;
    allBindings.Resize(bindingCount);

    spvReflectEnumerateDescriptorBindings(pModule, &bindingCount, allBindings.Data());

    HashMap<uint32, Array<SpvReflectDescriptorBinding*>> setToBindings;

    for (SpvReflectDescriptorBinding* binding : allBindings)
    {
        setToBindings[binding->set].PushBack(binding);
    }

    for (DescriptorSetDeclaration& setDecl : inputGroup.elements)
    {
        if (setDecl.flags & DescriptorSetDeclarationFlags::REFERENCE)
        {
            continue;
        }

        auto setToBindingsIt = setToBindings.Find(setDecl.setIndex);

        if (setToBindingsIt == setToBindings.End())
        {
            outErrorMessages.PushBack(HYP_FORMAT("Set #{} has no reflection info present", setDecl.setIndex));

            continue;
        }

        Array<SpvReflectDescriptorBinding*>& bindings = setToBindingsIt->second;

        for (auto& inputs : setDecl.slots)
        {
            for (ShaderInput& input : inputs)
            {
                auto bindingIt = bindings.FindIf([&input](SpvReflectDescriptorBinding* binding)
                    {
                        return Memory::StrCmp(binding->name, *input.name) == 0;
                    });

                if (bindingIt == bindings.End())
                {
                    outErrorMessages.PushBack(HYP_FORMAT("Binding for descriptor '{}' not found in reflection info", input.name));

                    continue;
                }

                SpvReflectDescriptorBinding* binding = *bindingIt;
                

                switch (binding->descriptor_type)
                {
                case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    ReflectConstantBuffer(binding, input);
                    break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    ReflectStructuredBuffer(binding, input);
                    break;
                default:
                    break;
                }
            }
        }
    }
}

} // namespace SPIRV

static void ReflectSPIRV(SpvReflectShaderModule* pModule, ShaderInputGroup& inputGroup, Array<String>& outErrorMessages)
{
    SPIRV::ReflectSPIRV(pModule, inputGroup, outErrorMessages);
}

#endif // HYP_SPIRV_REFLECT

} // namespace ShaderCompilerUtil

} // namespace Hyperion
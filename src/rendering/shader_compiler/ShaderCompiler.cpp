/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/shader_compiler/ShaderCompiler.hpp>
#include <util/ini/INIFile.hpp>
#include <core/filesystem/FsUtil.hpp>
#include <core/json/JSON.hpp>
#include <core/utilities/ByteUtil.hpp>

#include <core/Defines.hpp>

#include <core/utilities/ForEach.hpp>

#include <core/utilities/Time.hpp>

#include <core/object/HypData.hpp>

#include <core/containers/Stack.hpp>

#include <core/functional/Proc.hpp>

#include <core/algorithm/Map.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <asset/Assets.hpp>
#include <core/io/ByteWriter.hpp>

#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <core/math/MathUtil.hpp>

#include <Engine.hpp>

#include <rendering/backend/RenderConfig.hpp>

#ifdef HYP_GLSLANG

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Include/Types.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/MachineIndependent/reflection.h>

#endif

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(ShaderCompiler, Core);

static const bool g_should_compile_missing_variants = true;

// #define HYP_SHADER_COMPILER_LOGGING

#pragma region Helpers

static String BuildDescriptorTableDefines(const DescriptorTableDeclaration& descriptor_table_declaration)
{
    String descriptor_table_defines;

    // Generate descriptor table defines
    for (const DescriptorSetDeclaration& descriptor_set_declaration : descriptor_table_declaration.elements)
    {
        const DescriptorSetDeclaration* descriptor_set_declaration_ptr = &descriptor_set_declaration;

        const uint32 set_index = descriptor_table_declaration.GetDescriptorSetIndex(descriptor_set_declaration.name);
        AssertThrow(set_index != -1);

        descriptor_table_defines += "#define HYP_DESCRIPTOR_SET_INDEX_" + String(descriptor_set_declaration.name.LookupString()) + " " + String::ToString(set_index) + "\n";

        if (descriptor_set_declaration.flags[DescriptorSetDeclarationFlags::REFERENCE])
        {
            const DescriptorSetDeclaration* referenced_descriptor_set_declaration = renderer::GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(descriptor_set_declaration.name);
            AssertThrow(referenced_descriptor_set_declaration != nullptr);

            descriptor_set_declaration_ptr = referenced_descriptor_set_declaration;
        }

        for (const Array<DescriptorDeclaration>& descriptor_declarations : descriptor_set_declaration_ptr->slots)
        {
            for (const DescriptorDeclaration& descriptor_declaration : descriptor_declarations)
            {
                const uint32 flat_index = descriptor_set_declaration_ptr->CalculateFlatIndex(descriptor_declaration.slot, descriptor_declaration.name);
                AssertThrow(flat_index != uint32(-1));

                descriptor_table_defines += "\t#define HYP_DESCRIPTOR_INDEX_" + String(descriptor_set_declaration_ptr->name.LookupString()) + "_" + descriptor_declaration.name.LookupString() + " " + String::ToString(flat_index) + "\n";
            }
        }
    }

    return descriptor_table_defines;
}

static String BuildPreamble(const ShaderProperties& properties)
{
    String preamble;

    for (const VertexAttribute::Type attribute_type : properties.GetRequiredVertexAttributes().BuildAttributes())
    {
        preamble += String("#define HYP_ATTRIBUTE_") + VertexAttribute::mapping[attribute_type].name + "\n";
    }

    // We do not do the same for Optional attributes, as they have not been instantiated at this point in time.
    // before compiling the shader, they should have all been made Required.

    for (const ShaderProperty& property : properties.GetPropertySet())
    {
        if (property.name.Empty())
        {
            continue;
        }

        preamble += "#define " + property.name + "\n";
    }

    return preamble;
}

static String BuildPreamble(const ShaderProperties& properties, const DescriptorTableDeclaration& descriptor_table_declaration)
{
    return BuildDescriptorTableDefines(descriptor_table_declaration) + "\n\n" + BuildPreamble(properties);
}

#pragma endregion Helpers

#pragma region DescriptorUsageSet

DescriptorTableDeclaration DescriptorUsageSet::BuildDescriptorTableDeclaration() const
{
    DescriptorTableDeclaration table;

    for (const DescriptorUsage& descriptor_usage : elements)
    {
        AssertThrowMsg(descriptor_usage.slot != renderer::DescriptorSlot::DESCRIPTOR_SLOT_NONE && descriptor_usage.slot < renderer::DescriptorSlot::DESCRIPTOR_SLOT_MAX,
            "Descriptor usage %s has invalid slot %d", descriptor_usage.descriptor_name.LookupString(), descriptor_usage.slot);

        DescriptorSetDeclaration* descriptor_set_declaration = table.FindDescriptorSetDeclaration(descriptor_usage.set_name);

        // check if this descriptor set is defined in the static descriptor table
        // if it is, we can use those definitions
        // otherwise, it is a 'custom' descriptor set
        DescriptorSetDeclaration* static_descriptor_set_declaration = renderer::GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(descriptor_usage.set_name);

        if (static_descriptor_set_declaration != nullptr)
        {
            AssertThrowMsg(
                static_descriptor_set_declaration->FindDescriptorDeclaration(descriptor_usage.descriptor_name) != nullptr,
                "Descriptor set %s is defined in the static descriptor table, but the descriptor %s is not",
                descriptor_usage.set_name.LookupString(),
                descriptor_usage.descriptor_name.LookupString());

            if (!descriptor_set_declaration)
            {
                const uint32 set_index = uint32(table.elements.Size());

                DescriptorSetDeclaration new_descriptor_set_declaration(set_index, static_descriptor_set_declaration->name);
                new_descriptor_set_declaration.flags = static_descriptor_set_declaration->flags | DescriptorSetDeclarationFlags::REFERENCE;

                table.AddDescriptorSetDeclaration(std::move(new_descriptor_set_declaration));
            }

            continue;
        }

        if (!descriptor_set_declaration)
        {
            const uint32 set_index = uint32(table.elements.Size());

            descriptor_set_declaration = table.AddDescriptorSetDeclaration(DescriptorSetDeclaration(set_index, descriptor_usage.set_name));
        }

        DescriptorDeclaration desc {
            descriptor_usage.slot,
            descriptor_usage.descriptor_name,
            descriptor_usage.GetCount(),
            descriptor_usage.GetSize(),
            bool(descriptor_usage.flags & DESCRIPTOR_USAGE_FLAG_DYNAMIC)
        };

        if (auto* existing_decl = descriptor_set_declaration->FindDescriptorDeclaration(descriptor_usage.descriptor_name))
        {
            // Already exists, just update the slot
            *existing_decl = std::move(desc);
        }
        else
        {
            descriptor_set_declaration->AddDescriptorDeclaration(std::move(desc));
        }
    }

    return table;
}

#pragma endregion DescriptorUsageSet

#pragma region SPRIV Compilation

#if defined(HYP_VULKAN) && defined(HYP_GLSLANG)

static TBuiltInResource DefaultResources()
{
    return {
        /* .MaxLights = */ 32,
        /* .MaxClipPlanes = */ 6,
        /* .MaxTextureUnits = */ 32,
        /* .MaxTextureCoords = */ 32,
        /* .MaxVertexAttribs = */ 64,
        /* .MaxVertexUniformComponents = */ 4096,
        /* .MaxVaryingFloats = */ 64,
        /* .MaxVertexTextureImageUnits = */ 32,
        /* .MaxCombinedTextureImageUnits = */ 80,
        /* .MaxTextureImageUnits = */ 32,
        /* .MaxFragmentUniformComponents = */ 4096,
        /* .MaxDrawBuffers = */ 32,
        /* .MaxVertexUniformVectors = */ 128,
        /* .MaxVaryingVectors = */ 8,
        /* .MaxFragmentUniformVectors = */ 16,
        /* .MaxVertexOutputVectors = */ 16,
        /* .MaxFragmentInputVectors = */ 15,
        /* .MinProgramTexelOffset = */ -8,
        /* .MaxProgramTexelOffset = */ 7,
        /* .MaxClipDistances = */ 8,
        /* .MaxComputeWorkGroupCountX = */ 65535,
        /* .MaxComputeWorkGroupCountY = */ 65535,
        /* .MaxComputeWorkGroupCountZ = */ 65535,
        /* .MaxComputeWorkGroupSizeX = */ 1024,
        /* .MaxComputeWorkGroupSizeY = */ 1024,
        /* .MaxComputeWorkGroupSizeZ = */ 64,
        /* .MaxComputeUniformComponents = */ 1024,
        /* .MaxComputeTextureImageUnits = */ 16,
        /* .MaxComputeImageUniforms = */ 8,
        /* .MaxComputeAtomicCounters = */ 8,
        /* .MaxComputeAtomicCounterBuffers = */ 1,
        /* .MaxVaryingComponents = */ 60,
        /* .MaxVertexOutputComponents = */ 64,
        /* .MaxGeometryInputComponents = */ 64,
        /* .MaxGeometryOutputComponents = */ 128,
        /* .MaxFragmentInputComponents = */ 128,
        /* .MaxImageUnits = */ 8,
        /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
        /* .MaxCombinedShaderOutputResources = */ 8,
        /* .MaxImageSamples = */ 0,
        /* .MaxVertexImageUniforms = */ 0,
        /* .MaxTessControlImageUniforms = */ 0,
        /* .MaxTessEvaluationImageUniforms = */ 0,
        /* .MaxGeometryImageUniforms = */ 0,
        /* .MaxFragmentImageUniforms = */ 8,
        /* .MaxCombinedImageUniforms = */ 8,
        /* .MaxGeometryTextureImageUnits = */ 16,
        /* .MaxGeometryOutputVertices = */ 256,
        /* .MaxGeometryTotalOutputComponents = */ 1024,
        /* .MaxGeometryUniformComponents = */ 1024,
        /* .MaxGeometryVaryingComponents = */ 64,
        /* .MaxTessControlInputComponents = */ 128,
        /* .MaxTessControlOutputComponents = */ 128,
        /* .MaxTessControlTextureImageUnits = */ 16,
        /* .MaxTessControlUniformComponents = */ 1024,
        /* .MaxTessControlTotalOutputComponents = */ 4096,
        /* .MaxTessEvaluationInputComponents = */ 128,
        /* .MaxTessEvaluationOutputComponents = */ 128,
        /* .MaxTessEvaluationTextureImageUnits = */ 16,
        /* .MaxTessEvaluationUniformComponents = */ 1024,
        /* .MaxTessPatchComponents = */ 120,
        /* .MaxPatchVertices = */ 32,
        /* .MaxTessGenLevel = */ 64,
        /* .MaxViewports = */ 16,
        /* .MaxVertexAtomicCounters = */ 0,
        /* .MaxTessControlAtomicCounters = */ 0,
        /* .MaxTessEvaluationAtomicCounters = */ 0,
        /* .MaxGeometryAtomicCounters = */ 0,
        /* .MaxFragmentAtomicCounters = */ 8,
        /* .MaxCombinedAtomicCounters = */ 8,
        /* .MaxAtomicCounterBindings = */ 1,
        /* .MaxVertexAtomicCounterBuffers = */ 0,
        /* .MaxTessControlAtomicCounterBuffers = */ 0,
        /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
        /* .MaxGeometryAtomicCounterBuffers = */ 0,
        /* .MaxFragmentAtomicCounterBuffers = */ 1,
        /* .MaxCombinedAtomicCounterBuffers = */ 1,
        /* .MaxAtomicCounterBufferSize = */ 16384,
        /* .MaxTransformFeedbackBuffers = */ 4,
        /* .MaxTransformFeedbackInterleavedComponents = */ 64,
        /* .MaxCullDistances = */ 8,
        /* .MaxCombinedClipAndCullDistances = */ 8,
        /* .MaxSamples = */ 4,
        /* .maxMeshOutputVerticesNV = */ 256,
        /* .maxMeshOutputPrimitivesNV = */ 512,
        /* .maxMeshWorkGroupSizeX_NV = */ 32,
        /* .maxMeshWorkGroupSizeY_NV = */ 1,
        /* .maxMeshWorkGroupSizeZ_NV = */ 1,
        /* .maxTaskWorkGroupSizeX_NV = */ 32,
        /* .maxTaskWorkGroupSizeY_NV = */ 1,
        /* .maxTaskWorkGroupSizeZ_NV = */ 1,
        /* .maxMeshViewCountNV = */ 4,
        /* .maxMeshOutputVerticesEXT = */ 256,
        /* .maxMeshOutputPrimitivesEXT = */ 256,
        /* .maxMeshWorkGroupSizeX_EXT = */ 128,
        /* .maxMeshWorkGroupSizeY_EXT = */ 128,
        /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
        /* .maxTaskWorkGroupSizeX_EXT = */ 128,
        /* .maxTaskWorkGroupSizeY_EXT = */ 128,
        /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
        /* .maxMeshViewCountEXT = */ 4,
        /* .maxDualSourceDrawBuffersEXT = */ 1,

        /* .limits = */ {
            /* .nonInductiveForLoops = */ 1,
            /* .whileLoops = */ 1,
            /* .doWhileLoops = */ 1,
            /* .generalUniformIndexing = */ 1,
            /* .generalAttributeMatrixVectorIndexing = */ 1,
            /* .generalVaryingIndexing = */ 1,
            /* .generalSamplerIndexing = */ 1,
            /* .generalVariableIndexing = */ 1,
            /* .generalConstantMatrixVectorIndexing = */ 1,
        }
    };
}

static bool PreprocessShaderSource(
    ShaderModuleType type,
    ShaderLanguage language,
    String preamble,
    String source,
    String filename,
    String& out_preprocessed_source,
    Array<String>& out_error_messages)
{

#define GLSL_ERROR(level, error_message, ...)                                  \
    {                                                                          \
        HYP_LOG(ShaderCompiler, level, error_message, ##__VA_ARGS__);          \
        out_error_messages.PushBack(HYP_FORMAT(error_message, ##__VA_ARGS__)); \
    }

    auto default_resources = DefaultResources();

    glslang_stage_t stage;
    String stage_string;

    switch (type)
    {
    case ShaderModuleType::VERTEX:
        stage = GLSLANG_STAGE_VERTEX;
        stage_string = "VERTEX_SHADER";
        break;
    case ShaderModuleType::FRAGMENT:
        stage = GLSLANG_STAGE_FRAGMENT;
        stage_string = "FRAGMENT_SHADER";
        break;
    case ShaderModuleType::GEOMETRY:
        stage = GLSLANG_STAGE_GEOMETRY;
        stage_string = "GEOMETRY_SHADER";
        break;
    case ShaderModuleType::COMPUTE:
        stage = GLSLANG_STAGE_COMPUTE;
        stage_string = "COMPUTE_SHADER";
        break;
    case ShaderModuleType::TASK:
        stage = GLSLANG_STAGE_TASK_NV;
        stage_string = "TASK_SHADER";
        break;
    case ShaderModuleType::MESH:
        stage = GLSLANG_STAGE_MESH_NV;
        stage_string = "MESH_SHADER";
        break;
    case ShaderModuleType::TESS_CONTROL:
        stage = GLSLANG_STAGE_TESSCONTROL;
        stage_string = "TESS_CONTROL_SHADER";
        break;
    case ShaderModuleType::TESS_EVAL:
        stage = GLSLANG_STAGE_TESSEVALUATION;
        stage_string = "TESS_EVAL_SHADER";
        break;
    case ShaderModuleType::RAY_GEN:
        stage = GLSLANG_STAGE_RAYGEN_NV;
        stage_string = "RAY_GEN_SHADER";
        break;
    case ShaderModuleType::RAY_INTERSECT:
        stage = GLSLANG_STAGE_INTERSECT_NV;
        stage_string = "RAY_INTERSECT_SHADER";
        break;
    case ShaderModuleType::RAY_ANY_HIT:
        stage = GLSLANG_STAGE_ANYHIT_NV;
        stage_string = "RAY_ANY_HIT_SHADER";
        break;
    case ShaderModuleType::RAY_CLOSEST_HIT:
        stage = GLSLANG_STAGE_CLOSESTHIT_NV;
        stage_string = "RAY_CLOSEST_HIT_SHADER";
        break;
    case ShaderModuleType::RAY_MISS:
        stage = GLSLANG_STAGE_MISS_NV;
        stage_string = "RAY_MISS_SHADER";
        break;
    default:
        HYP_THROW("Invalid shader type");
        break;
    }

    uint32 vulkan_api_version = MathUtil::Max(HYP_VULKAN_API_VERSION, VK_API_VERSION_1_1);
    uint32 spirv_api_version = GLSLANG_TARGET_SPV_1_2;
    uint32 spirv_version = 450;

    if (IsRaytracingShaderModule(type))
    {
        vulkan_api_version = MathUtil::Max(vulkan_api_version, VK_API_VERSION_1_2);
        spirv_api_version = MathUtil::Max(spirv_api_version, GLSLANG_TARGET_SPV_1_4);
        spirv_version = MathUtil::Max(spirv_version, 460);
    }

    struct CallbacksContext
    {
        String filename;

        Stack<Proc<void()>> deleters;

        ~CallbacksContext()
        {
            // Run all deleters to free memory allocated in callbacks
            while (!deleters.Empty())
            {
                deleters.Pop()();
            }
        }
    } callbacks_context;

    callbacks_context.filename = filename;

    glslang_input_t input {
        .language = language == ShaderLanguage::HLSL ? GLSLANG_SOURCE_HLSL : GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = static_cast<glslang_target_client_version_t>(vulkan_api_version),
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = static_cast<glslang_target_language_version_t>(spirv_api_version),
        .code = source.Data(),
        .default_version = int(spirv_version),
        .default_profile = GLSLANG_CORE_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = reinterpret_cast<const glslang_resource_t*>(&default_resources),
        .callbacks_ctx = &callbacks_context
    };

    input.callbacks.include_local = [](void* ctx, const char* header_name, const char* includer_name, size_t include_depth) -> glsl_include_result_t*
    {
        CallbacksContext* callbacks_context = static_cast<CallbacksContext*>(ctx);

        const FilePath base_path = FilePath(callbacks_context->filename).BasePath();

        const FilePath dir = include_depth > 1
            ? FilePath(includer_name).BasePath()
            : g_asset_manager->GetBasePath() / FilePath::Relative(base_path, g_asset_manager->GetBasePath());

        const FilePath path = dir / header_name;

        BufferedReader reader;

        if (!path.Open(reader))
        {
            HYP_LOG(ShaderCompiler, Warning, "Failed to open include file {}", path);

            return nullptr;
        }

        String lines_joined = String::Join(reader.ReadAllLines(), '\n');

        glsl_include_result_t* result = new glsl_include_result_t;

        char* header_name_str = new char[path.Size() + 1];
        Memory::MemCpy(header_name_str, path.Data(), path.Size() + 1);
        result->header_name = header_name_str;

        char* header_data_str = new char[lines_joined.Size() + 1];
        Memory::MemCpy(header_data_str, lines_joined.Data(), lines_joined.Size() + 1);
        result->header_data = header_data_str;

        result->header_length = lines_joined.Size();

        callbacks_context->deleters.Push([result]
            {
                delete[] result->header_name;
                delete[] result->header_data;
                delete result;
            });

        return result;
    };

    glslang_shader_t* shader = glslang_shader_create(&input);

    if (stage_string.Any())
    {
        preamble += "\n#ifndef " + stage_string + "\n#define " + stage_string + "\n#endif\n";
    }

    glslang_shader_set_preamble(shader, preamble.Data());

    if (!glslang_shader_preprocess(shader, &input))
    {
        GLSL_ERROR(Error, "GLSL preprocessing failed {}", filename);
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_log(shader));
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_debug_log(shader));

        glslang_shader_delete(shader);

        return false;
    }

    out_preprocessed_source = glslang_shader_get_preprocessed_code(shader);

    // HYP_LOG(ShaderCompiler, Debug, "Preprocessed source for {}: Before: \n{}\nAfter:\n{}", filename, source, out_preprocessed_source);

    glslang_shader_delete(shader);

#undef GLSL_ERROR

    return true;
}

static ByteBuffer CompileToSPIRV(
    ShaderModuleType type,
    ShaderLanguage language,
    DescriptorUsageSet& descriptor_usages,
    String source,
    String filename,
    const ShaderProperties& properties,
    Array<String>& error_messages)
{

#define GLSL_ERROR(level, error_message, ...)                              \
    {                                                                      \
        HYP_LOG(ShaderCompiler, level, error_message, ##__VA_ARGS__);      \
        error_messages.PushBack(HYP_FORMAT(error_message, ##__VA_ARGS__)); \
    }

    auto default_resources = DefaultResources();

    glslang_stage_t stage;
    String stage_string;

    switch (type)
    {
    case ShaderModuleType::VERTEX:
        stage = GLSLANG_STAGE_VERTEX;
        stage_string = "VERTEX_SHADER";
        break;
    case ShaderModuleType::FRAGMENT:
        stage = GLSLANG_STAGE_FRAGMENT;
        stage_string = "FRAGMENT_SHADER";
        break;
    case ShaderModuleType::GEOMETRY:
        stage = GLSLANG_STAGE_GEOMETRY;
        stage_string = "GEOMETRY_SHADER";
        break;
    case ShaderModuleType::COMPUTE:
        stage = GLSLANG_STAGE_COMPUTE;
        stage_string = "COMPUTE_SHADER";
        break;
    case ShaderModuleType::TASK:
        stage = GLSLANG_STAGE_TASK_NV;
        stage_string = "TASK_SHADER";
        break;
    case ShaderModuleType::MESH:
        stage = GLSLANG_STAGE_MESH_NV;
        stage_string = "MESH_SHADER";
        break;
    case ShaderModuleType::TESS_CONTROL:
        stage = GLSLANG_STAGE_TESSCONTROL;
        stage_string = "TESS_CONTROL_SHADER";
        break;
    case ShaderModuleType::TESS_EVAL:
        stage = GLSLANG_STAGE_TESSEVALUATION;
        stage_string = "TESS_EVAL_SHADER";
        break;
    case ShaderModuleType::RAY_GEN:
        stage = GLSLANG_STAGE_RAYGEN_NV;
        stage_string = "RAY_GEN_SHADER";
        break;
    case ShaderModuleType::RAY_INTERSECT:
        stage = GLSLANG_STAGE_INTERSECT_NV;
        stage_string = "RAY_INTERSECT_SHADER";
        break;
    case ShaderModuleType::RAY_ANY_HIT:
        stage = GLSLANG_STAGE_ANYHIT_NV;
        stage_string = "RAY_ANY_HIT_SHADER";
        break;
    case ShaderModuleType::RAY_CLOSEST_HIT:
        stage = GLSLANG_STAGE_CLOSESTHIT_NV;
        stage_string = "RAY_CLOSEST_HIT_SHADER";
        break;
    case ShaderModuleType::RAY_MISS:
        stage = GLSLANG_STAGE_MISS_NV;
        stage_string = "RAY_MISS_SHADER";
        break;
    default:
        HYP_THROW("Invalid shader type");
        break;
    }

    uint32 vulkan_api_version = MathUtil::Max(HYP_VULKAN_API_VERSION, VK_API_VERSION_1_1);
    uint32 spirv_api_version = GLSLANG_TARGET_SPV_1_2;
    uint32 spirv_version = 450;

    if (IsRaytracingShaderModule(type))
    {
        vulkan_api_version = MathUtil::Max(vulkan_api_version, VK_API_VERSION_1_2);
        spirv_api_version = MathUtil::Max(spirv_api_version, GLSLANG_TARGET_SPV_1_4);
        spirv_version = MathUtil::Max(spirv_version, 460);
    }

    glslang_input_t input {
        .language = language == ShaderLanguage::HLSL ? GLSLANG_SOURCE_HLSL : GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = static_cast<glslang_target_client_version_t>(vulkan_api_version),
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = static_cast<glslang_target_language_version_t>(spirv_api_version),
        .code = source.Data(),
        .default_version = int(spirv_version),
        .default_profile = GLSLANG_CORE_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = reinterpret_cast<const glslang_resource_t*>(&default_resources),
        .callbacks_ctx = nullptr
    };

    glslang_shader_t* shader = glslang_shader_create(&input);

    String preamble = BuildDescriptorTableDefines(descriptor_usages.BuildDescriptorTableDeclaration());

    glslang_shader_set_preamble(shader, preamble.Data());

    if (!glslang_shader_preprocess(shader, &input))
    {
        GLSL_ERROR(Error, "GLSL preprocessing failed {}", filename);
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_log(shader));
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_debug_log(shader));

        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    if (!glslang_shader_parse(shader, &input))
    {
        GLSL_ERROR(Error, "GLSL parsing failed {}", filename);
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_log(shader));
        GLSL_ERROR(Error, "{}", glslang_shader_get_info_debug_log(shader));

        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
    {
        GLSL_ERROR(Error, "GLSL linking failed {} {}", filename, source);
        GLSL_ERROR(Error, "{}", glslang_program_get_info_log(program));
        GLSL_ERROR(Error, "{}", glslang_program_get_info_debug_log(program));

        glslang_program_delete(program);
        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    glslang::TProgram* cpp_program = glslang_get_cpp_program(program);
    if (!cpp_program->buildReflection())
    {
        GLSL_ERROR(Error, "Failed to build shader reflection!");
    }

    glslang_spv_options_t spv_options {};
    spv_options.disable_optimizer = false;
    spv_options.validate = true;

    glslang_program_SPIRV_generate_with_options(program, stage, &spv_options);

    // @TODO Add info from descriptor usages

    for (int i = 0; i < cpp_program->getNumUniformBlocks(); i++)
    {
        const auto& uniform_block = cpp_program->getUniformBlock(i);

        const glslang::TType* type = uniform_block.getType();
        AssertThrow(type != nullptr);

        DescriptorUsage* descriptor_usage = descriptor_usages.Find(CreateWeakNameFromDynamicString(uniform_block.name.data()));

        if (descriptor_usage != nullptr)
        {
            Proc<void(const glslang::TType*, DescriptorUsageType&)> HandleType;

            HandleType = [&HandleType](const glslang::TType* type, DescriptorUsageType& out_descriptor_usage_type)
            {
                if (type->isStruct())
                {
                    for (auto it = type->getStruct()->begin(); it != type->getStruct()->end(); ++it)
                    {
                        String field_type_name;

                        if (it->type->isStruct())
                        {
                            field_type_name = it->type->getTypeName().data();
                        }
                        else
                        {
                            field_type_name = it->type->getCompleteString(true, false, false, true).data();
                        }

                        HandleType(
                            it->type,
                            out_descriptor_usage_type.AddField(
                                                         CreateNameFromDynamicString(it->type->getFieldName().data()),
                                                         DescriptorUsageType(CreateNameFromDynamicString(field_type_name)))
                                .second);
                    }
                }
            };

            HandleType(type, descriptor_usage->type);
            descriptor_usage->type.size = uniform_block.size;
        }
    }

    ByteBuffer shader_module(glslang_program_SPIRV_get_size(program) * sizeof(uint32));
    glslang_program_SPIRV_get(program, reinterpret_cast<uint32*>(shader_module.Data()));

    const char* spirv_messages = glslang_program_SPIRV_get_messages(program);

    if (spirv_messages)
    {
        GLSL_ERROR(Error, "{}:\n{}", filename, spirv_messages);
    }

    glslang_program_delete(program);
    glslang_shader_delete(shader);

#undef GLSL_ERROR

    return shader_module;
}

#else

static bool PreprocessShaderSource(
    ShaderModuleType type,
    ShaderLanguage language,
    String preamble,
    String source,
    String filename,
    String& out_preprocessed_source,
    Array<String>& out_error_messages)
{
    out_preprocessed_source = source;

    return true;
}

static ByteBuffer CompileToSPIRV(
    ShaderModuleType type,
    ShaderLanguage language,
    DescriptorUsageSet& descriptor_usages,
    String source,
    String filename,
    const ShaderProperties& properties,
    Array<String>& error_messages)
{
    return ByteBuffer();
}

#endif

#pragma endregion SPRIV Compilation

struct LoadedSourceFile
{
    ShaderModuleType type;
    ShaderLanguage language;
    ShaderCompiler::SourceFile file;
    Time last_modified_timestamp;
    String source;

    FilePath GetOutputFilepath(const FilePath& base_path, const ShaderDefinition& shader_definition) const
    {
        HashCode hc;
        hc.Add(file.path);
        hc.Add(shader_definition.GetHashCode());

        return base_path / "data/compiled_shaders/tmp" / FilePath(file.path).Basename() + "_" + (String::ToString(hc.Value()) + ".spirv");
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(language);
        hc.Add(file);
        hc.Add(last_modified_timestamp);
        hc.Add(source);

        return hc;
    }
};

static const FlatMap<String, ShaderModuleType> shader_type_names = {
    { "vert", ShaderModuleType::VERTEX },
    { "frag", ShaderModuleType::FRAGMENT },
    { "geom", ShaderModuleType::GEOMETRY },
    { "comp", ShaderModuleType::COMPUTE },
    { "rgen", ShaderModuleType::RAY_GEN },
    { "rchit", ShaderModuleType::RAY_CLOSEST_HIT },
    { "rahit", ShaderModuleType::RAY_ANY_HIT },
    { "rmiss", ShaderModuleType::RAY_MISS },
    { "rint", ShaderModuleType::RAY_INTERSECT },
    { "tesc", ShaderModuleType::TESS_CONTROL },
    { "mesh", ShaderModuleType::MESH },
    { "task", ShaderModuleType::TASK }
};

static bool FindVertexAttributeForDefinition(const String& name, VertexAttribute::Type& out_type)
{
    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++)
    {
        const auto it = VertexAttribute::mapping.KeyValueAt(i);

        if (name == it.second.name)
        {
            out_type = it.first;

            return true;
        }
    }

    return false;
}

static void ForEachPermutation(
    const ShaderProperties& versions,
    const ProcRef<void(const ShaderProperties&)>& callback,
    bool parallel)
{
    Array<ShaderProperty> variable_properties;
    Array<ShaderProperty> static_properties;
    Array<ShaderProperty> value_groups;

    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++)
    {
        const auto& kv = VertexAttribute::mapping.KeyValueAt(i);

        if (!kv.second.name)
        {
            continue;
        }

        if (versions.HasRequiredVertexAttribute(kv.first))
        {
            static_properties.PushBack(ShaderProperty(kv.first));
        }
        else if (versions.HasOptionalVertexAttribute(kv.first))
        {
            variable_properties.PushBack(ShaderProperty(kv.first));
        }
    }

    for (const ShaderProperty& property : versions.GetPropertySet())
    {
        if (property.IsValueGroup())
        {
            value_groups.PushBack(property);
        }
        else if (property.is_permutation)
        {
            variable_properties.PushBack(property);
        }
        else
        {
            static_properties.PushBack(property);
        }
    }

    const SizeType num_permutations = 1ull << variable_properties.Size();

    SizeType total_count = num_permutations;

    for (const ShaderProperty& value_group : value_groups)
    {
        total_count += value_group.possible_values.Size() * total_count;
    }

    Array<Array<ShaderProperty>> all_combinations;
    all_combinations.Reserve(total_count);

    for (SizeType i = 0; i < num_permutations; i++)
    {
        Array<ShaderProperty> current_properties;
        current_properties.Reserve(ByteUtil::BitCount(i) + static_properties.Size());
        current_properties.Concat(static_properties);

        for (SizeType j = 0; j < variable_properties.Size(); j++)
        {
            if (i & (1ull << j))
            {
                current_properties.PushBack(variable_properties[j]);
            }
        }

        all_combinations.PushBack(std::move(current_properties));
    }

    // now apply the value groups onto it
    for (const ShaderProperty& value_group : value_groups)
    {
        // each value group causes the # of combinations to grow by (num values in group) * (current number of combinations)
        Array<Array<ShaderProperty>> current_group_combinations;
        current_group_combinations.Resize(value_group.possible_values.Size() * all_combinations.Size());

        for (SizeType existing_combination_index = 0; existing_combination_index < all_combinations.Size(); existing_combination_index++)
        {
            for (SizeType value_index = 0; value_index < value_group.possible_values.Size(); value_index++)
            {
                ShaderProperty new_property(value_group.name + "_" + value_group.possible_values[value_index], false);

                // copy the current version of the array
                Array<ShaderProperty> merged_properties(all_combinations[existing_combination_index]);
                merged_properties.PushBack(std::move(new_property));

                current_group_combinations[existing_combination_index + (value_index * all_combinations.Size())] = std::move(merged_properties);
            }
        }

        all_combinations.Concat(std::move(current_group_combinations));
    }

    // #ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(
        ShaderCompiler,
        Debug,
        "Processing {} shader combinations:",
        all_combinations.Size());
    // #endif

    if (parallel)
    {
        TaskSystem::GetInstance().ParallelForEach(
            all_combinations,
            [&callback](const Array<ShaderProperty>& properties, uint32, uint32)
            {
                callback(ShaderProperties(properties));
            });
    }
    else
    {
        for (const Array<ShaderProperty>& properties : all_combinations)
        {
            callback(ShaderProperties(properties));
        }
    }
}

static bool LoadBatchFromFile(const FilePath& filepath, CompiledShaderBatch& out_batch)
{
    // read file if it already exists.
    fbom::FBOMReader reader { fbom::FBOMReaderConfig {} };

    fbom::FBOMResult err;

    HypData value;

    if ((err = reader.LoadFromFile(filepath, value)))
    {
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Failed to compile shader at path: {}\n\tMessage: {}",
            filepath,
            err.message);

        return false;
    }

    Optional<CompiledShaderBatch&> compiled_shader_batch_opt = value.TryGet<CompiledShaderBatch>();

    if (!compiled_shader_batch_opt.HasValue())
    {
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Failed to load compiled shader at path: {}\n\tMessage: {}",
            filepath,
            "Failed to deserialize CompiledShaderBatch");

        return false;
    }

    out_batch = *compiled_shader_batch_opt;

    return true;
}

#pragma region ShaderProperties

ShaderProperties& ShaderProperties::Set(const ShaderProperty& property, bool enabled)
{
    if (property.IsVertexAttribute())
    {
        VertexAttribute::Type type;

        if (!FindVertexAttributeForDefinition(property.GetValueString(), type))
        {
            HYP_LOG(
                ShaderCompiler,
                Error,
                "Invalid vertex attribute name for shader: {}",
                property.GetValueString());

            return *this;
        }

        if (property.IsOptionalVertexAttribute())
        {
            if (enabled)
            {
                m_optional_vertex_attributes |= type;
                m_optional_vertex_attributes &= ~m_required_vertex_attributes;
            }
            else
            {
                m_optional_vertex_attributes &= ~type;
            }
        }
        else
        {
            if (enabled)
            {
                m_required_vertex_attributes |= type;
                m_optional_vertex_attributes &= ~type;
            }
            else
            {
                m_required_vertex_attributes &= ~type;
            }

            m_needs_hash_code_recalculation = true;
        }
    }
    else
    {
        const auto it = m_props.Find(property);

        if (enabled)
        {
            if (it == m_props.End())
            {
                m_props.Insert(property);

                m_needs_hash_code_recalculation = true;
            }
            else
            {
                if (*it != property)
                {
                    *it = property;

                    m_needs_hash_code_recalculation = true;
                }
            }
        }
        else
        {
            if (it != m_props.End())
            {
                m_props.Erase(it);

                m_needs_hash_code_recalculation = true;
            }
        }
    }

    return *this;
}

#pragma endregion ShaderProperties

#pragma region ShaderCompiler

ShaderCompiler::ShaderCompiler()
    : m_definitions(nullptr)
{
#if HYP_GLSLANG
    ShInitialize();
#endif
}

ShaderCompiler::~ShaderCompiler()
{
#if HYP_GLSLANG
    ShFinalize();
#endif

    if (m_definitions)
    {
        delete m_definitions;
    }
}

void ShaderCompiler::GetPlatformSpecificProperties(ShaderProperties& properties) const
{
#if defined(HYP_VULKAN) && HYP_VULKAN
    properties.Set(ShaderProperty("HYP_VULKAN", false));

    constexpr uint32 vulkan_version = HYP_VULKAN_API_VERSION;

    switch (vulkan_version)
    {
    case VK_API_VERSION_1_1:
        properties.Set(ShaderProperty("HYP_VULKAN_1_1", false));
        break;
    case VK_API_VERSION_1_2:
        properties.Set(ShaderProperty("HYP_VULKAN_1_2", false));
        break;
#ifdef VK_API_VERSION_1_3
    case VK_API_VERSION_1_3:
        properties.Set(ShaderProperty("HYP_VULKAN_1_3", false));
        break;
#endif
    default:
        break;
    }
#elif defined(HYP_DX12) && HYP_DX12
    properties.Set(ShaderProperty("DX12", false));
#endif

#if defined(HYP_WINDOWS)
    properties.Set(ShaderProperty("HYP_WINDOWS", false));
#elif defined(HYP_LINUX)
    properties.Set(ShaderProperty("HYP_LINUX", false));
#elif defined(HYP_MACOS)
    properties.Set(ShaderProperty("HYP_MACOS", false));
#elif defined(HYP_IOS)
    properties.Set(ShaderProperty("HYP_IOS", false));
#endif

    if (g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported())
    {
        properties.Set(ShaderProperty("HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING", false));
    }

    if (g_rendering_api->GetRenderConfig().IsBindlessSupported())
    {
        properties.Set(ShaderProperty("HYP_FEATURES_BINDLESS_TEXTURES", false));
    }

    if (!g_rendering_api->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
    {
        properties.Set(ShaderProperty("HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA", false));
    }

    // props.Set(ShaderProperty("HYP_MAX_SHADOW_MAPS", false));
    // props.Set(ShaderProperty("HYP_MAX_BONES", false));
}

void ShaderCompiler::ParseDefinitionSection(
    const INIFile::Section& section,
    ShaderCompiler::Bundle& bundle)
{
    for (const auto& section_it : section)
    {
        if (section_it.first == "permute")
        {
            // set each property
            for (const auto& element : section_it.second.elements)
            {
                if (element.sub_elements.Any())
                {
                    bundle.versions.AddValueGroup(element.name, element.sub_elements);
                }
                else
                {
                    bundle.versions.AddPermutation(element.name);
                }
            }
        }
        else if (section_it.first == "entry_point")
        {
            bundle.entry_point_name = section_it.second.GetValue().name;
        }
        else if (shader_type_names.Contains(section_it.first))
        {
            bundle.sources[shader_type_names.At(section_it.first)] = SourceFile {
                g_asset_manager->GetBasePath() / "shaders" / section_it.second.GetValue().name
            };
        }
        else
        {
            HYP_LOG(
                ShaderCompiler,
                Warning,
                "Unknown property in shader definition file: {}\n",
                section_it.first);
        }
    }
}

bool ShaderCompiler::HandleCompiledShaderBatch(
    Bundle& bundle,
    const ShaderProperties& requested_properties,
    const FilePath& output_file_path,
    CompiledShaderBatch& batch)
{
    // Check that each version specified is present in the CompiledShaderBatch.
    // OR any src files have been changed since the object file was compiled.
    // if not, we need to recompile those versions.

    // TODO: Each individual item should have a timestamp internally set
    const Time object_file_last_modified = output_file_path.LastModifiedTimestamp();

    Time max_source_file_last_modified = Time(0);

    for (auto& source_file : bundle.sources)
    {
        max_source_file_last_modified = MathUtil::Max(max_source_file_last_modified, FilePath(source_file.second.path).LastModifiedTimestamp());
    }

    if (max_source_file_last_modified > object_file_last_modified)
    {
        HYP_LOG(
            ShaderCompiler,
            Info,
            "Source file in batch {} has been modified since the batch was last compiled, recompiling...",
            *bundle.name);

        batch = CompiledShaderBatch {};

        return CompileBundle(bundle, requested_properties, batch);
    }

    // find variants for the bundle that are not in the compiled batch
    Array<ShaderProperties> missing_variants;

    ForEachPermutation(bundle.versions, [&](const ShaderProperties& properties)
        {
            // get hashcode for this set of properties
            // only care about the property set (not vertex attributes), as we will only have access to those from the bundle
            // plus, changing vertex attributes will cause a recompile anyway due to shaders' file contents changing
            const HashCode properties_hash_code = properties.GetPropertySetHashCode();

            const auto it = batch.compiled_shaders.FindIf([properties_hash_code](const CompiledShader& item)
                {
                    return item.GetDefinition().GetProperties().GetPropertySetHashCode() == properties_hash_code;
                });

            if (it == batch.compiled_shaders.End())
            {
                missing_variants.PushBack(properties);
            }
        },
        false);

    const bool requested_found = batch.compiled_shaders.FindIf([requested_properties_hash_code = requested_properties.GetHashCode()](const CompiledShader& item)
                                     {
                                         return item.GetDefinition().GetProperties().GetHashCode() == requested_properties_hash_code;
                                     })
        != batch.compiled_shaders.End();

    if (missing_variants.Any() || !requested_found)
    {
        ShaderProperties all_properties;

        for (const CompiledShader& compiled_shader : batch.compiled_shaders)
        {
            all_properties.Merge(compiled_shader.GetDefinition().GetProperties());
        }

        String missing_variants_string;

        {
            SizeType index = 0;

            for (const ShaderProperties& missing_shader_properties : missing_variants)
            {
                missing_variants_string += String::ToString(missing_shader_properties.GetHashCode().Value()) + " - " + missing_shader_properties.ToString();

                if (index != missing_variants.Size() - 1)
                {
                    missing_variants_string += ",\n\t";
                }

                index++;
            }
        }

        // clear the batch if properties requested are missing.
        batch = CompiledShaderBatch {};

        if (g_should_compile_missing_variants && CanCompileShaders())
        {
            HYP_LOG(
                ShaderCompiler,
                Info,
                "Compiled shader is missing properties. Attempting to compile with the missing properties.\n\tRequested with properties:\n\t{} ({})\n\n\tMissing variants:\n\t{}\n\n\tAll found properties: {}",
                requested_properties.ToString(), (requested_found ? "found" : "not found"),
                missing_variants_string,
                all_properties.ToString());

            return CompileBundle(bundle, requested_properties, batch);
        }

        HYP_BREAKPOINT;

        return false;
    }

    return true;
}

bool ShaderCompiler::LoadOrCompileBatch(
    Name name,
    const ShaderProperties& properties,
    CompiledShaderBatch& batch)
{
    if (!CanCompileShaders())
    {
        HYP_LOG(
            ShaderCompiler,
            Warning,
            "Not compiled with GLSL compiler support... Shaders may become out of date.\n"
            "If any .hypshader files are missing, you may need to recompile the engine with glslang linked, "
            "so that they can be generated.");
    }

    if (!m_definitions || !m_definitions->IsValid())
    {
        // load for first time if no definitions loaded
        if (!LoadShaderDefinitions())
        {
            return false;
        }
    }

    const String name_string(name.LookupString());

    if (!m_definitions->HasSection(name_string))
    {
        // not in definitions file
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Section {} not found in shader definitions file",
            name);

        return false;
    }

    Bundle bundle { name };
    GetPlatformSpecificProperties(bundle.versions);

    // apply each permutable property from the definitions file
    const INIFile::Section& section = m_definitions->GetSection(name_string);
    ParseDefinitionSection(section, bundle);

    auto try_to_compile_on_missing = [&](const FilePath& output_file_path)
    {
        if (CanCompileShaders())
        {
            HYP_LOG(
                ShaderCompiler,
                Info,
                "Attempting to compile shader {}...",
                output_file_path);
        }
        else
        {
            HYP_LOG(
                ShaderCompiler,
                Error,
                "Failed to load compiled shader file: {}. The file could not be found.",
                output_file_path);

            return false;
        }

        if (!CompileBundle(bundle, properties, batch))
        {
            HYP_LOG(
                ShaderCompiler,
                Error,
                "Failed to compile shader bundle {}",
                name);

            return false;
        }

        return LoadBatchFromFile(output_file_path, batch);
    };

    const FilePath output_file_path = g_asset_manager->GetBasePath() / "data/compiled_shaders" / name_string + ".hypshader";

    if (output_file_path.Exists())
    {
        HYP_LOG(
            ShaderCompiler,
            Info,
            "Attempting to load compiled shader {}...",
            output_file_path);

        if (!LoadBatchFromFile(output_file_path, batch))
        {
            if (!try_to_compile_on_missing(output_file_path))
            {
                return false;
            }
        }
    }
    else if (!try_to_compile_on_missing(output_file_path))
    {
        return false;
    }

    return HandleCompiledShaderBatch(bundle, properties, output_file_path, batch);
}

bool ShaderCompiler::LoadShaderDefinitions(bool precompile_shaders)
{
    if (m_definitions && m_definitions->IsValid())
    {
        return true;
    }

    const FilePath data_path = g_asset_manager->GetBasePath() / "data/compiled_shaders";

    if (!data_path.Exists())
    {
        if (FileSystem::MkDir(data_path.Data()) != 0)
        {
            HYP_LOG(
                ShaderCompiler,
                Error,
                "Failed to create data path at {}",
                data_path);

            return false;
        }
    }

    if (m_definitions)
    {
        delete m_definitions;
    }

    m_definitions = new INIFile(g_asset_manager->GetBasePath() / "Shaders.ini");

    if (!m_definitions->IsValid())
    {
        HYP_LOG(
            ShaderCompiler,
            Warning,
            "Failed to load shader definitions file at path: {}",
            m_definitions->GetFilePath());

        delete m_definitions;
        m_definitions = nullptr;

        return false;
    }

    if (!precompile_shaders)
    {
        return true;
    }

    HYP_LOG(ShaderCompiler, Info, "Precompiling shaders...");

    Array<Bundle> bundles;

    // create a bundle for each section.
    for (const auto& it : m_definitions->GetSections())
    {
        const String& key = it.first;
        const INIFile::Section& section = it.second;

        const Name name_from_string = CreateNameFromDynamicString(ANSIString(key));

        Bundle bundle { name_from_string };

        ParseDefinitionSection(section, bundle);

        bundles.PushBack(std::move(bundle));
    }

    const bool supports_rt_shaders = g_rendering_api->GetRenderConfig().IsRaytracingSupported();

    HashMap<Bundle*, bool> results;
    Mutex results_mutex;

    // Compile all shaders ahead of time
    TaskSystem::GetInstance().ParallelForEach(
        bundles,
        [&](auto& bundle, uint32, uint32)
        {
            if (bundle.HasRTShaders() && !supports_rt_shaders)
            {
                HYP_LOG(
                    ShaderCompiler,
                    Warning,
                    "Not compiling shader bundle {} because it contains raytracing shaders and raytracing is not supported on this device.",
                    bundle.name);

                return;
            }

            if (bundle.HasVertexShader())
            {
                bundle.versions.Merge(ShaderProperties(static_mesh_vertex_attributes));
                bundle.versions.Merge(ShaderProperties(static_mesh_vertex_attributes | skeleton_vertex_attributes));
            }

            ForEachPermutation(bundle.versions.ToArray(), [&](const ShaderProperties& properties)
                {
                    CompiledShader compiled_shader;
                    bool result = GetCompiledShader(bundle.name, properties, compiled_shader);

                    Mutex::Guard guard(results_mutex);
                    results[&bundle] = result;
                },
                false); // true);
        });

    bool all_results = true;

    for (const auto& it : results)
    {
        if (!it.second)
        {
            String permutation_string;

            HYP_LOG(
                ShaderCompiler,
                Error,
                "{}: Loading of compiled shader failed with version hash {}",
                it.first->name,
                it.first->versions.GetHashCode().Value());

            all_results = false;
        }
    }

    return all_results;
}

bool ShaderCompiler::CanCompileShaders() const
{
    if (!g_engine->GetConfig().Get(CONFIG_SHADER_COMPILATION).GetBool())
    {
        return false;
    }

#ifdef HYP_GLSLANG
    return true;
#else
    return false;
#endif
}

ShaderCompiler::ProcessResult ShaderCompiler::ProcessShaderSource(
    ProcessShaderSourcePhase phase,
    ShaderModuleType type,
    ShaderLanguage language,
    const String& source,
    const String& filename,
    const ShaderProperties& properties)
{
    ProcessResult result;
    Array<String> lines;

    if (phase == ProcessShaderSourcePhase::AFTER_PREPROCESS)
    {
        String preprocessed_source;

        Array<String> preprocess_error_messages;
        const bool preprocess_result = PreprocessShaderSource(type, language, BuildPreamble(properties), source, filename, preprocessed_source, preprocess_error_messages);

        result.errors.Concat(Map(preprocess_error_messages, [](const String& error_message)
            {
                return ProcessError { error_message };
            }));

        if (!preprocess_result)
        {
            return result;
        }

        lines = preprocessed_source.Split('\n');
    }
    else
    {
        lines = source.Split('\n');
    }

    struct ParseCustomStatementResult
    {
        Array<String> args;
        String remaining;
    };

    auto parse_custom_statement = [](const String& start, const String& line) -> ParseCustomStatementResult
    {
        const String substr = line.Substr(start.Length());

        String args_string;

        int parentheses_depth = 0;
        SizeType index;

        // Note: using 'Size' and 'Data' to index -- not using utf-8 chars here.
        for (index = 0; index < substr.Size(); index++)
        {
            if (substr.Data()[index] == ')')
            {
                parentheses_depth--;
            }

            if (parentheses_depth > 0)
            {
                args_string.Append(substr.Data()[index]);
            }

            if (substr.Data()[index] == '(')
            {
                parentheses_depth++;
            }

            if (parentheses_depth <= 0)
            {
                break;
            }
        }

        Array<String> args = args_string.Split(',');

        for (String& arg : args)
        {
            arg = arg.Trimmed();
        }

        return {
            std::move(args),
            substr.Substr(index + 1)
        };
    };

    int last_attribute_location = -1;

    for (uint32 line_index = 0; line_index < lines.Size();)
    {
        const String line = lines[line_index++].Trimmed();

        switch (phase)
        {
        case ProcessShaderSourcePhase::BEFORE_PREPROCESS:
        {
            if (line.StartsWith("HYP_ATTRIBUTE"))
            {
                Array<String> parts = line.Split(' ');

                bool optional = false;

                if (parts.Size() != 3)
                {
                    result.errors.PushBack(ProcessError { "Invalid attribute: Requires format HYP_ATTRIBUTE(location) type name" });

                    break;
                }

                char ch;

                String attribute_keyword,
                    attribute_type,
                    attribute_location,
                    attribute_name;

                Optional<String> attribute_condition;

                {
                    SizeType index = 0;

                    while (index != parts.Front().Size() && (std::isalpha(ch = parts.Front()[index]) || ch == '_'))
                    {
                        attribute_keyword.Append(ch);
                        ++index;
                    }

                    if (attribute_keyword == "HYP_ATTRIBUTE_OPTIONAL")
                    {
                        optional = true;
                    }
                    else if (attribute_keyword == "HYP_ATTRIBUTE")
                    {
                        optional = false;
                    }
                    else
                    {
                        result.errors.PushBack(ProcessError { String("Invalid attribute, unknown attribute keyword `") + attribute_keyword + "`" });

                        break;
                    }

                    if (index != parts.Front().Size() && ((ch = parts.Front()[index]) == '('))
                    {
                        ++index;

                        // read integer string
                        while (index != parts.Front().Size() && std::isdigit(ch = parts.Front()[index]))
                        {
                            attribute_location.Append(ch);
                            ++index;
                        }

                        // if there is a comma, read the conditional define that we will use
                        if (index != parts.Front().Size() && ((ch = parts.Front()[index]) == ','))
                        {
                            ++index;

                            String condition;
                            while (index != parts.Front().Size() && (std::isalpha(ch = parts.Front()[index]) || ch == '_'))
                            {
                                condition.Append(ch);
                                ++index;
                            }

                            attribute_condition = condition;
                        }

                        if (index != parts.Front().Size() && ((ch = parts.Front()[index]) == ')'))
                        {
                            ++index;
                        }
                        else
                        {
                            result.errors.PushBack(ProcessError { "Invalid attribute, missing closing parenthesis" });

                            break;
                        }

                        if (attribute_location.Empty())
                        {
                            result.errors.PushBack(ProcessError { "Invalid attribute location" });

                            break;
                        }
                    }
                }

                for (SizeType index = 0; index < parts[1].Size() && (std::isalnum(ch = parts[1][index]) || ch == '_'); index++)
                {
                    attribute_type.Append(ch);
                }

                for (SizeType index = 0; index < parts[2].Size() && (std::isalnum(ch = parts[2][index]) || ch == '_'); index++)
                {
                    attribute_name.Append(ch);
                }

                VertexAttributeDefinition attribute_definition;
                attribute_definition.name = attribute_name;
                attribute_definition.type_class = attribute_type;
                attribute_definition.location = attribute_location.Any()
                    ? std::atoi(attribute_location.Data())
                    : last_attribute_location + 1;

                last_attribute_location = attribute_definition.location;

                if (optional)
                {
                    result.optional_attributes.PushBack(attribute_definition);

                    if (attribute_condition.HasValue())
                    {
                        result.processed_source += "#if defined(" + attribute_condition.Get() + ") && " + attribute_condition.Get() + "\n";

                        attribute_definition.condition = attribute_condition;
                    }
                    else
                    {
                        result.processed_source += "#ifdef HYP_ATTRIBUTE_" + attribute_definition.name + "\n";
                    }
                }
                else
                {
                    result.required_attributes.PushBack(attribute_definition);
                }

                result.processed_source += "layout(location=" + String::ToString(attribute_definition.location) + ") in " + attribute_definition.type_class + " " + attribute_definition.name + ";\n";

                if (optional)
                {
                    result.processed_source += "#endif\n";
                }

                continue;
            }

            break;
        }
        case ProcessShaderSourcePhase::AFTER_PREPROCESS:
        {
            if (line.StartsWith("HYP_DESCRIPTOR"))
            {
                String command_str;

                for (SizeType index = 0; index < line.Size(); index++)
                {
                    if (std::isalnum(line.Data()[index]) || line.Data()[index] == '_')
                    {
                        command_str.Append(line.Data()[index]);
                    }
                    else
                    {
                        break;
                    }
                }

                renderer::DescriptorSlot slot = renderer::DESCRIPTOR_SLOT_NONE;
                DescriptorUsageFlags flags = DESCRIPTOR_USAGE_FLAG_NONE;

                if (command_str == "HYP_DESCRIPTOR_SRV")
                {
                    slot = renderer::DESCRIPTOR_SLOT_SRV;
                }
                else if (command_str == "HYP_DESCRIPTOR_UAV")
                {
                    slot = renderer::DESCRIPTOR_SLOT_UAV;
                }
                else if (command_str == "HYP_DESCRIPTOR_CBUFF" || command_str == "HYP_DESCRIPTOR_CBUFF_DYNAMIC")
                {
                    slot = renderer::DESCRIPTOR_SLOT_CBUFF;

                    if (command_str == "HYP_DESCRIPTOR_CBUFF_DYNAMIC")
                    {
                        flags |= DESCRIPTOR_USAGE_FLAG_DYNAMIC;
                    }
                }
                else if (command_str == "HYP_DESCRIPTOR_SSBO" || command_str == "HYP_DESCRIPTOR_SSBO_DYNAMIC")
                {
                    slot = renderer::DESCRIPTOR_SLOT_SSBO;

                    if (command_str == "HYP_DESCRIPTOR_SSBO_DYNAMIC")
                    {
                        flags |= DESCRIPTOR_USAGE_FLAG_DYNAMIC;
                    }
                }
                else if (command_str == "HYP_DESCRIPTOR_ACCELERATION_STRUCTURE")
                {
                    slot = renderer::DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE;
                }
                else if (command_str == "HYP_DESCRIPTOR_SAMPLER")
                {
                    slot = renderer::DESCRIPTOR_SLOT_SAMPLER;
                }
                else
                {
                    result.errors.PushBack(ProcessError { "Invalid descriptor slot. Must match HYP_DESCRIPTOR_<Type> " });

                    break;
                }

                Array<String> parts = line.Split(' ');

                String descriptor_name,
                    set_name,
                    slot_str;

                HashMap<String, String> params;

                auto parse_result = parse_custom_statement(command_str, line);

                if (parse_result.args.Size() < 2)
                {
                    result.errors.PushBack(ProcessError { "Invalid descriptor: Requires format HYP_DESCRIPTOR_<TYPE>(set, name)" });

                    break;
                }

                set_name = parse_result.args[0];
                descriptor_name = parse_result.args[1];

                if (parse_result.args.Size() > 2)
                {
                    for (SizeType index = 2; index < parse_result.args.Size(); index++)
                    {
                        Array<String> split = parse_result.args[index].Split('=');

                        for (String& part : split)
                        {
                            part = part.Trimmed();
                        }

                        if (split.Size() != 2)
                        {
                            result.errors.PushBack(ProcessError { "Invalid parameter: Requires format <key>=<value>" });

                            break;
                        }

                        const String& key = split[0];
                        const String& value = split[1];

                        params[key] = value;
                    }
                }

                const DescriptorUsage usage {
                    slot,
                    CreateNameFromDynamicString(ANSIString(set_name)),
                    CreateNameFromDynamicString(ANSIString(descriptor_name)),
                    flags,
                    std::move(params)
                };

                String std_version = "std140";

                if (usage.params.Contains("standard"))
                {
                    std_version = usage.params.At("standard");
                }

                Array<String> additional_params;

                if (usage.params.Contains("format"))
                {
                    additional_params.PushBack(usage.params.At("format"));
                }

                // add std140, std430 etc. for buffer types.
                switch (usage.slot)
                {
                case renderer::DESCRIPTOR_SLOT_CBUFF: // fallthrough
                case renderer::DESCRIPTOR_SLOT_SSBO:  // fallthrough
                    if (usage.params.Contains("matrix_mode"))
                    {
                        additional_params.PushBack(usage.params.At("matrix_mode"));
                    }
                    else
                    {
                        additional_params.PushBack("row_major");
                    }

                    result.processed_source += "layout(" + std_version + ", set=HYP_DESCRIPTOR_SET_INDEX_" + set_name + ", binding=HYP_DESCRIPTOR_INDEX_" + set_name + "_" + descriptor_name + (additional_params.Any() ? (", " + String::Join(additional_params, ", ")) : String::empty) + ") " + parse_result.remaining + "\n";
                    break;
                default:
                    result.processed_source += "layout(set=HYP_DESCRIPTOR_SET_INDEX_" + set_name + ", binding=HYP_DESCRIPTOR_INDEX_" + set_name + "_" + descriptor_name + (additional_params.Any() ? (", " + String::Join(additional_params, ", ")) : String::empty) + ") " + parse_result.remaining + "\n";
                    break;
                }

                result.descriptor_usages.PushBack(usage);

                continue;
            }

            break;
        }
        }

        result.processed_source += line + '\n';
    }

#ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(ShaderCompiler, Info, "Processed source: {}", result.processed_source);
#endif

    return result;
}

bool ShaderCompiler::CompileBundle(
    Bundle& bundle,
    const ShaderProperties& additional_versions,
    CompiledShaderBatch& out)
{
    if (!CanCompileShaders())
    {
        return false;
    }

    // run with spirv-cross
    FileSystem::MkDir((g_asset_manager->GetBasePath() / "data/compiled_shaders/tmp").Data());

    Array<LoadedSourceFile> loaded_source_files;
    loaded_source_files.Resize(bundle.sources.Size());

    Array<Array<ProcessError>> process_errors;
    process_errors.Resize(bundle.sources.Size());

    Array<Array<VertexAttributeDefinition>> required_vertex_attributes;
    required_vertex_attributes.Resize(bundle.sources.Size());

    Array<Array<VertexAttributeDefinition>> optional_vertex_attributes;
    optional_vertex_attributes.Resize(bundle.sources.Size());

    TaskBatch task_batch;

    for (SizeType index = 0; index < bundle.sources.Size(); index++)
    {
        StaticMessage debug_name;
        debug_name.value = ANSIStringView(bundle.sources.AtIndex(index).second.path.Data());

        task_batch.AddTask([this, index, &bundle, &loaded_source_files, &process_errors, &required_vertex_attributes, &optional_vertex_attributes](...)
            {
                const auto& it = bundle.sources.AtIndex(index);

                const FilePath filepath = it.second.path;
                const ShaderLanguage language = filepath.EndsWith("hlsl") ? ShaderLanguage::HLSL : ShaderLanguage::GLSL;

                BufferedReader reader;

                if (!filepath.Open(reader))
                {
                    // could not open file!
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "Failed to open shader source file at {}",
                        filepath);

                    process_errors[index] = {
                        ProcessError { "Failed to open source file" }
                    };

                    return;
                }

                const ByteBuffer byte_buffer = reader.ReadBytes();
                String source_string(byte_buffer.ToByteView());

                // process shader source to extract vertex attributes.
                // runs before actual preprocessing
                ProcessResult result = ProcessShaderSource(ProcessShaderSourcePhase::BEFORE_PREPROCESS, it.first, language, source_string, filepath, {});

                if (result.errors.Any())
                {
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "{} shader processing errors:",
                        result.errors.Size());

                    process_errors[index] = result.errors;

                    return;
                }

                required_vertex_attributes[index] = result.required_attributes;
                optional_vertex_attributes[index] = result.optional_attributes;

                loaded_source_files[index] = LoadedSourceFile {
                    .type = it.first,
                    .language = language,
                    .file = it.second,
                    .last_modified_timestamp = filepath.LastModifiedTimestamp(),
                    .source = result.processed_source
                };
            });
    }

    if (Threads::IsOnThread(ThreadCategory::THREAD_CATEGORY_TASK))
    {
        // run on this thread if we are already in a task thread
        task_batch.ExecuteBlocking();
    }
    else
    {
        // Hack fix: task threads that are currently enqueueing RenderCommands can cause a deadlock,
        // if we are waiting on tasks to complete from the render thread.

        if (Threads::IsOnThread(g_render_thread))
        {
            task_batch.ExecuteBlocking();
        }
        else
        {
            TaskSystem::GetInstance().EnqueueBatch(&task_batch);
            task_batch.AwaitCompletion();
        }
    }

    Array<ProcessError> all_process_errors;

    for (const auto& error_list : process_errors)
    {
        all_process_errors.Concat(error_list);
    }

    if (!all_process_errors.Empty())
    {
        for (const ProcessError& error : all_process_errors)
        {
            HYP_LOG(
                ShaderCompiler,
                Error,
                "\t{}",
                error.error_message);
        }

        return false;
    }

    // // Merge all descriptor usages together
    // for (const Array<DescriptorUsage> &descriptor_usage_list : descriptor_usages) {
    //     bundle.descriptor_usages.Merge(descriptor_usage_list);
    // }

    // grab each defined property, and iterate over each combination
    ShaderProperties final_properties;
    final_properties.Merge(bundle.versions);

    { // Lookup vertex attribute names

        VertexAttributeSet required_vertex_attribute_set;
        VertexAttributeSet optional_vertex_attribute_set;

        for (const auto& definitions : required_vertex_attributes)
        {
            for (const VertexAttributeDefinition& definition : definitions)
            {
                VertexAttribute::Type type;

                if (!FindVertexAttributeForDefinition(definition.name, type))
                {
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "Invalid vertex attribute definition, {}",
                        definition.name);

                    continue;
                }

                required_vertex_attribute_set |= type;
            }
        }

        for (const auto& definitions : optional_vertex_attributes)
        {
            for (const VertexAttributeDefinition& definition : definitions)
            {
                VertexAttribute::Type type;

                if (!FindVertexAttributeForDefinition(definition.name, type))
                {
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "Invalid vertex attribute definition, {}",
                        definition.name);

                    continue;
                }

                optional_vertex_attribute_set |= type;
            }
        }

        final_properties.SetRequiredVertexAttributes(required_vertex_attribute_set);
        final_properties.SetOptionalVertexAttributes(optional_vertex_attribute_set);
    }

    // copy passed properties
    final_properties.Merge(additional_versions);

    HYP_LOG(
        ShaderCompiler,
        Info,
        "Compiling shader bundle for shader {}",
        bundle.name);

    // update versions to include vertex attribute properties
    bundle.versions = final_properties;

    Mutex fs_mutex;
    Mutex compiled_shaders_mutex;
    Mutex error_messages_mutex;

    AtomicVar<uint32> num_compiled_permutations { 0u };
    AtomicVar<uint32> num_errored_permutations { 0u };

    auto build_vertex_attribute_set = [](const Array<VertexAttributeDefinition>& definitions) -> VertexAttributeSet
    {
        VertexAttributeSet set;

        for (const VertexAttributeDefinition& definition : definitions)
        {
            VertexAttribute::Type type;

            if (!FindVertexAttributeForDefinition(definition.name, type))
            {
                HYP_LOG(
                    ShaderCompiler,
                    Error,
                    "Invalid vertex attribute definition, {}",
                    definition.name);

                continue;
            }

            set |= type;
        }

        return set;
    };

    // compile shader with each permutation of properties
    ForEachPermutation(final_properties, [&](const ShaderProperties& properties)
        {
            CompiledShader compiled_shader;

            compiled_shader.definition = ShaderDefinition {
                bundle.name,
                properties
            };

            compiled_shader.entry_point_name = bundle.entry_point_name;

            AssertThrow(compiled_shader.definition.IsValid());

            AtomicVar<bool> any_files_compiled { false };
            AtomicVar<bool> any_files_errored { false };

            Array<DescriptorUsageSet> descriptor_usage_sets_per_file;
            descriptor_usage_sets_per_file.Resize(loaded_source_files.Size());

            Array<String> processed_sources;
            processed_sources.Resize(loaded_source_files.Size());

            Array<Pair<FilePath, bool /* skip */>> filepaths;
            filepaths.Resize(loaded_source_files.Size());

            // load each source file, check if the output file exists, and if it does, check if it is older than the source file
            // if it is, we can reuse the file.
            // otherwise, we process the source and prepare it for compilation
            for (SizeType index = 0; index < loaded_source_files.Size(); index++)
            {
                const LoadedSourceFile& item = loaded_source_files[index];

                // check if a file exists w/ same hash
                const FilePath output_filepath = item.GetOutputFilepath(
                    g_asset_manager->GetBasePath(),
                    compiled_shader.definition);

                filepaths[index] = { output_filepath, false };

                // if (output_filepath.Exists()) {
                //     // file exists and is older than the original source file; no need to build
                //     if (output_filepath.LastModifiedTimestamp() >= item.last_modified_timestamp) {
                //         BufferedReader reader;

                //         if (output_filepath.Open(reader)) {
                //             compiled_shader.modules[item.type] = reader.ReadBytes();

                //             filepaths[index].second = true;

                //             return;
                //         }

                //         HYP_LOG(
                //             ShaderCompiler,
                //             Warning,
                //             "File {} seems valid for reuse but could not be opened. Attempting to rebuild...\n\tProperties: [{}]",
                //             output_filepath,
                //             properties.ToString()
                //         );
                //     }
                // }

                DescriptorUsageSet& descriptor_usages = descriptor_usage_sets_per_file[index];

                Array<String> error_messages;

                // set directory to the directory of the shader
                const FilePath dir = g_asset_manager->GetBasePath() / FilePath::Relative(FilePath(item.file.path).BasePath(), g_asset_manager->GetBasePath());

                String& processed_source = processed_sources[index];

                { // Process shader (preprocessing, custom statements, etc.)
                    ProcessResult process_result = ProcessShaderSource(ProcessShaderSourcePhase::AFTER_PREPROCESS, item.type, item.language, item.source, item.file.path, properties);

                    if (process_result.errors.Any())
                    {
                        HYP_LOG(
                            ShaderCompiler,
                            Error,
                            "{} shader processing errors:",
                            process_result.errors.Size());

                        Mutex::Guard guard(error_messages_mutex);

                        out.error_messages.Concat(Map(process_result.errors, &ProcessError::error_message));

                        any_files_errored.Set(true, MemoryOrder::RELAXED);

                        return;
                    }

                    descriptor_usages.Merge(std::move(process_result.descriptor_usages));

                    processed_source = process_result.processed_source;
                }
            }

            // merge all descriptor usages together for the source files before compiling.
            DescriptorUsageSet descriptor_usage_sets_merged;

            for (const DescriptorUsageSet& descriptor_usage_set : descriptor_usage_sets_per_file)
            {
                descriptor_usage_sets_merged.Merge(descriptor_usage_set);
            }

            descriptor_usage_sets_per_file.Clear();

            // final substitution of properties + compilation
            for (SizeType index = 0; index < loaded_source_files.Size(); index++)
            {
                const LoadedSourceFile& item = loaded_source_files[index];

                const Pair<FilePath, bool>& filepath_state = filepaths[index];

                // don't process these files
                if (filepath_state.second)
                {
                    return;
                }

                const FilePath& output_filepath = filepath_state.first;

                Array<String> error_messages;

                { // logging stuff
                    String variable_properties_string;
                    String static_properties_string;

                    for (const ShaderProperty& property : properties.ToArray())
                    {
                        if (property.is_permutation)
                        {
                            if (!variable_properties_string.Empty())
                            {
                                variable_properties_string += ", ";
                            }

                            variable_properties_string += property.name;
                        }
                        else
                        {
                            if (!static_properties_string.Empty())
                            {
                                static_properties_string += ", ";
                            }

                            static_properties_string += property.name;
                        }
                    }

                    // HYP_LOG(
                    //     ShaderCompiler,
                    //     Info,
                    //     "Compiling shader {}\n\tVariable properties: [{}]\n\tStatic properties: [{}]\n\tProperties hash: {}",
                    //     output_filepath,
                    //     variable_properties_string,
                    //     static_properties_string,
                    //     properties.GetHashCode().Value()
                    // );
                }

                ByteBuffer byte_buffer = CompileToSPIRV(item.type, item.language, descriptor_usage_sets_merged, processed_sources[index], item.file.path, properties, error_messages);

                if (byte_buffer.Empty())
                {
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "Failed to compile file {} with version hash {}",
                        item.file.path,
                        properties.GetHashCode().Value());

                    Mutex::Guard guard(error_messages_mutex);
                    out.error_messages.Concat(std::move(error_messages));

                    any_files_errored.Set(true, MemoryOrder::RELAXED);

                    return;
                }

                compiled_shader.descriptor_usage_set = descriptor_usage_sets_merged;

                // write the spirv to the output file
                FileByteWriter spirv_writer(output_filepath.Data());

                if (!spirv_writer.IsOpen())
                {
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "Could not open file {} for writing!",
                        output_filepath);

                    any_files_errored.Set(true, MemoryOrder::RELAXED);

                    return;
                }

                spirv_writer.Write(byte_buffer.Data(), byte_buffer.Size());
                spirv_writer.Close();

                any_files_compiled.Set(true, MemoryOrder::RELAXED);

                compiled_shader.modules[item.type] = std::move(byte_buffer);
            }

            num_compiled_permutations.Increment(uint32(!any_files_errored.Get(MemoryOrder::RELAXED) && any_files_compiled.Get(MemoryOrder::RELAXED)), MemoryOrder::RELAXED);
            num_errored_permutations.Increment(uint32(any_files_errored.Get(MemoryOrder::RELAXED)), MemoryOrder::RELAXED);

            compiled_shader.descriptor_table_declaration = compiled_shader.descriptor_usage_set.BuildDescriptorTableDeclaration();

            Mutex::Guard guard(compiled_shaders_mutex);
            out.compiled_shaders.PushBack(std::move(compiled_shader));
        },
        false); // true);

    if (num_errored_permutations.Get(MemoryOrder::RELAXED))
    {
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Failed to compile {} permutations of shader {}",
            num_errored_permutations.Get(MemoryOrder::RELAXED),
            bundle.name);

        return false;
    }

    const FilePath final_output_path = g_asset_manager->GetBasePath() / "data/compiled_shaders" / String(bundle.name.LookupString()) + ".hypshader";

    FileByteWriter byte_writer(final_output_path.Data());

    fbom::FBOMWriter serializer { fbom::FBOMWriterConfig {} };

    if (fbom::FBOMResult err = serializer.Append(out))
    {
        HYP_BREAKPOINT_DEBUG_MODE;

        return false;
    }

    if (fbom::FBOMResult err = serializer.Emit(&byte_writer))
    {
        HYP_BREAKPOINT_DEBUG_MODE;

        return false;
    }

    byte_writer.Close();

    m_cache.Set(bundle.name, out);

    if (num_compiled_permutations.Get(MemoryOrder::RELAXED) != 0)
    {
        HYP_LOG(
            ShaderCompiler,
            Info,
            "Compiled {} new variants for shader {} to: {}",
            num_compiled_permutations.Get(MemoryOrder::RELAXED),
            bundle.name,
            final_output_path);
    }

    return true;
}

CompiledShader ShaderCompiler::GetCompiledShader(Name name)
{
    ShaderProperties properties {};

    return GetCompiledShader(name, properties);
}

CompiledShader ShaderCompiler::GetCompiledShader(Name name, const ShaderProperties& properties)
{
    CompiledShader compiled_shader;

    GetCompiledShader(
        name,
        properties,
        compiled_shader);

    return compiled_shader;
}

bool ShaderCompiler::GetCompiledShader(
    Name name,
    const ShaderProperties& properties,
    CompiledShader& out)
{
    ShaderProperties final_properties;
    GetPlatformSpecificProperties(final_properties);
    final_properties.Merge(properties);

    const HashCode final_properties_hash = final_properties.GetHashCode();

    if (m_cache.GetShaderInstance(name, final_properties_hash.Value(), out))
    {
        return true;
    }

    CompiledShaderBatch batch;

    if (!LoadOrCompileBatch(name, final_properties, batch))
    {
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Failed to attempt loading of shader batch: {}\n\tRequested instance with properties: [{}]",
            name,
            final_properties.ToString());

        return false;
    }

    // set in cache so we can use it later
    m_cache.Set(name, batch);

    // make sure we properly created it

    auto it = batch.compiled_shaders.FindIf([final_properties_hash](const CompiledShader& compiled_shader)
        {
            if (!compiled_shader.IsValid())
            {
                return false;
            }

            return compiled_shader.GetDefinition().GetProperties().GetHashCode() == final_properties_hash;
        });

    if (it == batch.compiled_shaders.End())
    {
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Hash calculation for shader {} does not match {}! Invalid shader property combination.\n\tRequested instance with properties: [{}]",
            name,
            final_properties_hash.Value(),
            final_properties.ToString());

        return false;
    }

    out = *it;

    HYP_LOG(
        ShaderCompiler,
        Debug,
        "Selected shader {} for hash {}.\n\tRequested instance with properties: [{}]",
        name,
        final_properties_hash.Value(),
        final_properties.ToString());

    AssertThrow(out.GetDefinition().IsValid());

    return true;
}

#pragma endregion ShaderCompiler

} // namespace hyperion

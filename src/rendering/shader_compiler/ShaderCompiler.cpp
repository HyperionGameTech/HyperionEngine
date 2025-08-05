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

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <asset/Assets.hpp>
#include <core/io/ByteWriter.hpp>

#include <core/serialization/fbom/FBOMWriter.hpp>
#include <core/serialization/fbom/FBOMReader.hpp>

#include <core/math/MathUtil.hpp>

#include <EngineGlobals.hpp>
#include <HyperionEngine.hpp>
#include <Engine.hpp>

#include <rendering/RenderConfig.hpp>
#include <rendering/RenderBackend.hpp>

#define HYP_SHADER_REFLECTION

#ifdef HYP_GLSLANG

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Include/Types.h>
#include <glslang/Public/ShaderLang.h>

#ifdef HYP_SHADER_REFLECTION
#include <glslang/MachineIndependent/reflection.h>
#endif

#endif

#ifdef HYP_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(ShaderCompiler, Core);

static const bool g_shouldCompileMissingVariants = true;

// #define HYP_SHADER_COMPILER_LOGGING

#pragma region Helpers

static String BuildDescriptorTableDefines(const DescriptorTableDeclaration& descriptorTableDeclaration)
{
    String descriptorTableDefines;

    // Generate descriptor table defines
    for (const DescriptorSetDeclaration& descriptorSetDeclaration : descriptorTableDeclaration.elements)
    {
        const DescriptorSetDeclaration* descriptorSetDeclarationPtr = &descriptorSetDeclaration;

        const uint32 setIndex = descriptorTableDeclaration.GetDescriptorSetIndex(descriptorSetDeclaration.name);
        Assert(setIndex != -1);

        descriptorTableDefines += "#define HYP_DESCRIPTOR_SET_INDEX_" + String(descriptorSetDeclaration.name.LookupString()) + " " + String::ToString(setIndex) + "\n";

        if (descriptorSetDeclaration.flags[DescriptorSetDeclarationFlags::REFERENCE])
        {
            const DescriptorSetDeclaration* referencedDescriptorSetDeclaration = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(descriptorSetDeclaration.name);
            Assert(referencedDescriptorSetDeclaration != nullptr);

            descriptorSetDeclarationPtr = referencedDescriptorSetDeclaration;
        }

        for (const Array<DescriptorDeclaration>& descriptorDeclarations : descriptorSetDeclarationPtr->slots)
        {
            for (const DescriptorDeclaration& descriptorDeclaration : descriptorDeclarations)
            {
                const uint32 flatIndex = descriptorSetDeclarationPtr->CalculateFlatIndex(descriptorDeclaration.slot, descriptorDeclaration.name);
                Assert(flatIndex != uint32(-1));

                descriptorTableDefines += "\t#define HYP_DESCRIPTOR_INDEX_" + String(descriptorSetDeclarationPtr->name.LookupString()) + "_" + descriptorDeclaration.name.LookupString() + " " + String::ToString(flatIndex) + "\n";
            }
        }
    }

    return descriptorTableDefines;
}

static String BuildPreamble(const ShaderProperties& properties)
{
    String preamble;

    for (const VertexAttribute::Type attributeType : properties.GetRequiredVertexAttributes().BuildAttributes())
    {
        preamble += String("#define HYP_ATTRIBUTE_") + VertexAttribute::mapping[attributeType].name + "\n";
    }

    // We do not do the same for Optional attributes, as they have not been instantiated at this point in time.
    // before compiling the shader, they should have all been made Required.

    for (const ShaderProperty& property : properties.GetPropertySet())
    {
        if (!property.name.IsValid())
        {
            continue;
        }

        // property has a value -- use #define <name> <value>
        if (property.HasValue())
        {
            preamble += HYP_FORMAT("#define {} {}\n", property.name, property.GetValueString());

            continue;
        }

        // no value set, treat it as boolean true (1)
        preamble += HYP_FORMAT("#define {} 1\n", property.name);
    }

    return preamble;
}

static String BuildPreamble(const ShaderProperties& properties, const DescriptorTableDeclaration& descriptorTableDeclaration)
{
    return BuildDescriptorTableDefines(descriptorTableDeclaration) + "\n\n" + BuildPreamble(properties);
}

#pragma endregion Helpers

#pragma region DescriptorUsageSet

DescriptorTableDeclaration DescriptorUsageSet::BuildDescriptorTableDeclaration() const
{
    DescriptorTableDeclaration table;

    for (const DescriptorUsage& descriptorUsage : elements)
    {
        Assert(descriptorUsage.slot != DescriptorSlot::DESCRIPTOR_SLOT_NONE && descriptorUsage.slot < DescriptorSlot::DESCRIPTOR_SLOT_MAX,
            "Descriptor usage {} has invalid slot {}", descriptorUsage.descriptorName.LookupString(), descriptorUsage.slot);

        DescriptorSetDeclaration* descriptorSetDeclaration = table.FindDescriptorSetDeclaration(descriptorUsage.setName);

        // check if this descriptor set is defined in the static descriptor table
        // if it is, we can use those definitions
        // otherwise, it is a 'custom' descriptor set
        DescriptorSetDeclaration* staticDescriptorSetDeclaration = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(descriptorUsage.setName);

        if (staticDescriptorSetDeclaration != nullptr)
        {
            Assert(
                staticDescriptorSetDeclaration->FindDescriptorDeclaration(descriptorUsage.descriptorName) != nullptr,
                "Descriptor set {} is defined in the static descriptor table, but the descriptor {} is not",
                descriptorUsage.setName, descriptorUsage.descriptorName);

            if (!descriptorSetDeclaration)
            {
                const uint32 setIndex = uint32(table.elements.Size());

                DescriptorSetDeclaration newDescriptorSetDeclaration(setIndex, staticDescriptorSetDeclaration->name);
                newDescriptorSetDeclaration.flags = staticDescriptorSetDeclaration->flags | DescriptorSetDeclarationFlags::REFERENCE;

                table.AddDescriptorSetDeclaration(std::move(newDescriptorSetDeclaration));
            }

            continue;
        }

        if (!descriptorSetDeclaration)
        {
            const uint32 setIndex = uint32(table.elements.Size());

            descriptorSetDeclaration = table.AddDescriptorSetDeclaration(DescriptorSetDeclaration(setIndex, descriptorUsage.setName));
        }

        DescriptorDeclaration desc {
            descriptorUsage.slot,
            descriptorUsage.descriptorName,
            descriptorUsage.GetCount(),
            descriptorUsage.GetSize(),
            bool(descriptorUsage.flags & DESCRIPTOR_USAGE_FLAG_DYNAMIC)
        };

        if (auto* existingDecl = descriptorSetDeclaration->FindDescriptorDeclaration(descriptorUsage.descriptorName))
        {
            // Already exists, just update the slot
            *existingDecl = std::move(desc);
        }
        else
        {
            descriptorSetDeclaration->AddDescriptorDeclaration(std::move(desc));
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
    String& outPreprocessedSource,
    Array<String>& outErrorMessages)
{

#define GLSL_ERROR(level, errorMessage, ...)                                \
    {                                                                       \
        HYP_LOG(ShaderCompiler, level, errorMessage, ##__VA_ARGS__);        \
        outErrorMessages.PushBack(HYP_FORMAT(errorMessage, ##__VA_ARGS__)); \
    }

    auto defaultResources = DefaultResources();

    glslang_stage_t stage;
    String stageString;

    switch (type)
    {
    case SMT_VERTEX:
        stage = GLSLANG_STAGE_VERTEX;
        stageString = "VERTEX_SHADER";
        break;
    case SMT_FRAGMENT:
        stage = GLSLANG_STAGE_FRAGMENT;
        stageString = "FRAGMENT_SHADER";
        break;
    case SMT_GEOMETRY:
        stage = GLSLANG_STAGE_GEOMETRY;
        stageString = "GEOMETRY_SHADER";
        break;
    case SMT_COMPUTE:
        stage = GLSLANG_STAGE_COMPUTE;
        stageString = "COMPUTE_SHADER";
        break;
    case SMT_TASK:
        stage = GLSLANG_STAGE_TASK_NV;
        stageString = "TASK_SHADER";
        break;
    case SMT_MESH:
        stage = GLSLANG_STAGE_MESH_NV;
        stageString = "MESH_SHADER";
        break;
    case SMT_TESS_CONTROL:
        stage = GLSLANG_STAGE_TESSCONTROL;
        stageString = "TESS_CONTROL_SHADER";
        break;
    case SMT_TESS_EVAL:
        stage = GLSLANG_STAGE_TESSEVALUATION;
        stageString = "TESS_EVAL_SHADER";
        break;
    case SMT_RAY_GEN:
        stage = GLSLANG_STAGE_RAYGEN_NV;
        stageString = "RAY_GEN_SHADER";
        break;
    case SMT_RAY_INTERSECT:
        stage = GLSLANG_STAGE_INTERSECT_NV;
        stageString = "RAY_INTERSECT_SHADER";
        break;
    case SMT_RAY_ANY_HIT:
        stage = GLSLANG_STAGE_ANYHIT_NV;
        stageString = "RAY_ANY_HIT_SHADER";
        break;
    case SMT_RAY_CLOSEST_HIT:
        stage = GLSLANG_STAGE_CLOSESTHIT_NV;
        stageString = "RAY_CLOSEST_HIT_SHADER";
        break;
    case SMT_RAY_MISS:
        stage = GLSLANG_STAGE_MISS_NV;
        stageString = "RAY_MISS_SHADER";
        break;
    default:
        HYP_THROW("Invalid shader type");
        break;
    }

    uint32 vulkanApiVersion = MathUtil::Max(HYP_VULKAN_API_VERSION, VK_API_VERSION_1_1);
    uint32 spirvApiVersion = GLSLANG_TARGET_SPV_1_2;
    uint32 spirvVersion = 450;

    if (IsRaytracingShaderModule(type))
    {
        vulkanApiVersion = MathUtil::Max(vulkanApiVersion, VK_API_VERSION_1_2);
        spirvApiVersion = MathUtil::Max(spirvApiVersion, GLSLANG_TARGET_SPV_1_4);
        spirvVersion = MathUtil::Max(spirvVersion, 460);
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
    } callbacksContext;

    callbacksContext.filename = filename;

    glslang_input_t input {
        .language = language == ShaderLanguage::HLSL ? GLSLANG_SOURCE_HLSL : GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = static_cast<glslang_target_client_version_t>(vulkanApiVersion),
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = static_cast<glslang_target_language_version_t>(spirvApiVersion),
        .code = source.Data(),
        .default_version = int(spirvVersion),
        .default_profile = GLSLANG_CORE_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = reinterpret_cast<const glslang_resource_t*>(&defaultResources),
        .callbacks_ctx = &callbacksContext
    };

    input.callbacks.include_local = [](void* ctx, const char* headerName, const char* includerName, size_t includeDepth) -> glsl_include_result_t*
    {
        CallbacksContext* callbacksContext = static_cast<CallbacksContext*>(ctx);

        const FilePath basePath = FilePath(callbacksContext->filename).BasePath();

        const FilePath dir = includeDepth > 1
            ? FilePath(includerName).BasePath()
            : GetResourceDirectory() / FilePath::Relative(basePath, GetResourceDirectory());

        const FilePath path = dir / headerName;

        if (!path.Exists())
        {
            HYP_LOG(ShaderCompiler, Warning, "File at path {} does not exist, cannot include file {}", path, headerName);

            return nullptr;
        }

        FileBufferedReaderSource source { path };
        BufferedReader reader { &source };

        if (!reader.IsOpen())
        {
            HYP_LOG(ShaderCompiler, Warning, "Failed to open include file {}", path);

            return nullptr;
        }

        String linesJoined = String::Join(reader.ReadAllLines(), '\n');

        glsl_include_result_t* result = new glsl_include_result_t;

        char* headerNameStr = new char[path.Size() + 1];
        Memory::MemCpy(headerNameStr, path.Data(), path.Size() + 1);
        result->header_name = headerNameStr;

        char* headerDataStr = new char[linesJoined.Size() + 1];
        Memory::MemCpy(headerDataStr, linesJoined.Data(), linesJoined.Size() + 1);
        result->header_data = headerDataStr;

        result->header_length = linesJoined.Size();

        callbacksContext->deleters.Push([result]
            {
                delete[] result->header_name;
                delete[] result->header_data;
                delete result;
            });

        return result;
    };

    glslang_shader_t* shader = glslang_shader_create(&input);

    if (stageString.Any())
    {
        preamble += "\n#ifndef " + stageString + "\n#define " + stageString + "\n#endif\n";
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

    outPreprocessedSource = glslang_shader_get_preprocessed_code(shader);

    // HYP_LOG(ShaderCompiler, Debug, "Preprocessed source for {}: Before: \n{}\nAfter:\n{}", filename, source, outPreprocessedSource);

    glslang_shader_delete(shader);

#undef GLSL_ERROR

    return true;
}

static ByteBuffer CompileToSPIRV(
    ShaderModuleType type,
    ShaderLanguage language,
    DescriptorUsageSet& descriptorUsages,
    String source,
    String filename,
    const ShaderProperties& properties,
    Array<String>& errorMessages)
{

#define GLSL_ERROR(level, errorMessage, ...)                             \
    {                                                                    \
        HYP_LOG(ShaderCompiler, level, errorMessage, ##__VA_ARGS__);     \
        errorMessages.PushBack(HYP_FORMAT(errorMessage, ##__VA_ARGS__)); \
    }

    auto defaultResources = DefaultResources();

    glslang_stage_t stage;
    String stageString;

    switch (type)
    {
    case SMT_VERTEX:
        stage = GLSLANG_STAGE_VERTEX;
        stageString = "VERTEX_SHADER";
        break;
    case SMT_FRAGMENT:
        stage = GLSLANG_STAGE_FRAGMENT;
        stageString = "FRAGMENT_SHADER";
        break;
    case SMT_GEOMETRY:
        stage = GLSLANG_STAGE_GEOMETRY;
        stageString = "GEOMETRY_SHADER";
        break;
    case SMT_COMPUTE:
        stage = GLSLANG_STAGE_COMPUTE;
        stageString = "COMPUTE_SHADER";
        break;
    case SMT_TASK:
        stage = GLSLANG_STAGE_TASK_NV;
        stageString = "TASK_SHADER";
        break;
    case SMT_MESH:
        stage = GLSLANG_STAGE_MESH_NV;
        stageString = "MESH_SHADER";
        break;
    case SMT_TESS_CONTROL:
        stage = GLSLANG_STAGE_TESSCONTROL;
        stageString = "TESS_CONTROL_SHADER";
        break;
    case SMT_TESS_EVAL:
        stage = GLSLANG_STAGE_TESSEVALUATION;
        stageString = "TESS_EVAL_SHADER";
        break;
    case SMT_RAY_GEN:
        stage = GLSLANG_STAGE_RAYGEN_NV;
        stageString = "RAY_GEN_SHADER";
        break;
    case SMT_RAY_INTERSECT:
        stage = GLSLANG_STAGE_INTERSECT_NV;
        stageString = "RAY_INTERSECT_SHADER";
        break;
    case SMT_RAY_ANY_HIT:
        stage = GLSLANG_STAGE_ANYHIT_NV;
        stageString = "RAY_ANY_HIT_SHADER";
        break;
    case SMT_RAY_CLOSEST_HIT:
        stage = GLSLANG_STAGE_CLOSESTHIT_NV;
        stageString = "RAY_CLOSEST_HIT_SHADER";
        break;
    case SMT_RAY_MISS:
        stage = GLSLANG_STAGE_MISS_NV;
        stageString = "RAY_MISS_SHADER";
        break;
    default:
        HYP_THROW("Invalid shader type");
        break;
    }

    uint32 vulkanApiVersion = MathUtil::Max(HYP_VULKAN_API_VERSION, VK_API_VERSION_1_1);
    uint32 spirvApiVersion = GLSLANG_TARGET_SPV_1_2;
    uint32 spirvVersion = 450;

    if (IsRaytracingShaderModule(type))
    {
        vulkanApiVersion = MathUtil::Max(vulkanApiVersion, VK_API_VERSION_1_2);
        spirvApiVersion = MathUtil::Max(spirvApiVersion, GLSLANG_TARGET_SPV_1_4);
        spirvVersion = MathUtil::Max(spirvVersion, 460);
    }

    glslang_input_t input {
        .language = language == ShaderLanguage::HLSL ? GLSLANG_SOURCE_HLSL : GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = static_cast<glslang_target_client_version_t>(vulkanApiVersion),
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = static_cast<glslang_target_language_version_t>(spirvApiVersion),
        .code = source.Data(),
        .default_version = int(spirvVersion),
        .default_profile = GLSLANG_CORE_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = reinterpret_cast<const glslang_resource_t*>(&defaultResources),
        .callbacks_ctx = nullptr
    };

    glslang_shader_t* shader = glslang_shader_create(&input);

    String preamble = BuildDescriptorTableDefines(descriptorUsages.BuildDescriptorTableDeclaration());

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

#ifdef HYP_SHADER_REFLECTION
    glslang::TProgram* cppProgram = glslang_get_cpp_program(program);
    if (!cppProgram->buildReflection())
    {
        GLSL_ERROR(Error, "Failed to build shader reflection!");
    }
#endif

    glslang_spv_options_t spvOptions {};
    spvOptions.disable_optimizer = false;
    spvOptions.validate = true;

    glslang_program_SPIRV_generate_with_options(program, stage, &spvOptions);

#ifdef HYP_SHADER_REFLECTION
    for (int i = 0; i < cppProgram->getNumUniformBlocks(); i++)
    {
        const auto& uniformBlock = cppProgram->getUniformBlock(i);

        const glslang::TType* type = uniformBlock.getType();
        Assert(type != nullptr);

        DescriptorUsage* descriptorUsage = descriptorUsages.Find(CreateWeakNameFromDynamicString(uniformBlock.name.data()));

        if (descriptorUsage != nullptr)
        {
            Proc<void(const glslang::TType*, DescriptorUsageType&)> HandleType;

            HandleType = [&HandleType](const glslang::TType* type, DescriptorUsageType& outDescriptorUsageType)
            {
                if (type->isStruct())
                {
                    for (auto it = type->getStruct()->begin(); it != type->getStruct()->end(); ++it)
                    {
                        String fieldTypeName;

                        if (it->type->isStruct())
                        {
                            fieldTypeName = it->type->getTypeName().data();
                        }
                        else
                        {
                            fieldTypeName = it->type->getCompleteString(true, false, false, true).data();
                        }
                        
                        auto& field = outDescriptorUsageType.AddField(
                            CreateNameFromDynamicString(it->type->getFieldName().data()),
                            DescriptorUsageType(CreateNameFromDynamicString(fieldTypeName))).second;

                        HandleType(it->type, field);
                    }
                }
            };

            HandleType(type, descriptorUsage->type);
            descriptorUsage->type.size = uniformBlock.size;
        }
    }
#endif

    ByteBuffer shaderModule(glslang_program_SPIRV_get_size(program) * sizeof(uint32));
    glslang_program_SPIRV_get(program, reinterpret_cast<uint32*>(shaderModule.Data()));

    const char* spirvMessages = glslang_program_SPIRV_get_messages(program);

    if (spirvMessages)
    {
        GLSL_ERROR(Error, "{}:\n{}", filename, spirvMessages);
    }

    glslang_program_delete(program);
    glslang_shader_delete(shader);

#undef GLSL_ERROR

    return shaderModule;
}

#else

static bool PreprocessShaderSource(
    ShaderModuleType type,
    ShaderLanguage language,
    String preamble,
    String source,
    String filename,
    String& outPreprocessedSource,
    Array<String>& outErrorMessages)
{
    outPreprocessedSource = source;

    return true;
}

static ByteBuffer CompileToSPIRV(
    ShaderModuleType type,
    ShaderLanguage language,
    DescriptorUsageSet& descriptorUsages,
    String source,
    String filename,
    const ShaderProperties& properties,
    Array<String>& errorMessages)
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
    Time lastModifiedTimestamp;
    String source;

    FilePath GetOutputFilepath(const FilePath& basePath, const ShaderDefinition& shaderDefinition) const
    {
        HashCode hc;
        hc.Add(file.path);
        hc.Add(shaderDefinition.GetHashCode());

        return basePath / "data/compiled_shaders/tmp" / FilePath(file.path).Basename() + "_" + (String::ToString(hc.Value()) + ".spirv");
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(language);
        hc.Add(file);
        hc.Add(lastModifiedTimestamp);
        hc.Add(source);

        return hc;
    }
};

static const FlatMap<String, ShaderModuleType> shaderTypeNames = {
    { "vert", SMT_VERTEX },
    { "frag", SMT_FRAGMENT },
    { "geom", SMT_GEOMETRY },
    { "comp", SMT_COMPUTE },
    { "rgen", SMT_RAY_GEN },
    { "rchit", SMT_RAY_CLOSEST_HIT },
    { "rahit", SMT_RAY_ANY_HIT },
    { "rmiss", SMT_RAY_MISS },
    { "rint", SMT_RAY_INTERSECT },
    { "tesc", SMT_TESS_CONTROL },
    { "mesh", SMT_MESH },
    { "task", SMT_TASK }
};

static bool FindVertexAttributeForDefinition(const String& name, VertexAttribute::Type& outType)
{
    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++)
    {
        const auto it = VertexAttribute::mapping.KeyValueAt(i);

        if (name == it.second.name)
        {
            outType = it.first;

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
    Array<ShaderProperty> variableProperties;
    Array<ShaderProperty> staticProperties;
    Array<ShaderProperty> valueGroups;

    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++)
    {
        const auto& kv = VertexAttribute::mapping.KeyValueAt(i);

        if (!kv.second.name)
        {
            continue;
        }

        if (versions.HasRequiredVertexAttribute(kv.first))
        {
            staticProperties.PushBack(ShaderProperty(kv.first));
        }
        else if (versions.HasOptionalVertexAttribute(kv.first))
        {
            variableProperties.PushBack(ShaderProperty(kv.first));
        }
    }

    for (const ShaderProperty& property : versions.GetPropertySet())
    {
        if (property.IsValueGroup())
        {
            valueGroups.PushBack(property);
        }
        else if (property.isPermutation)
        {
            variableProperties.PushBack(property);
        }
        else
        {
            staticProperties.PushBack(property);
        }
    }

    const SizeType numPermutations = 1ull << variableProperties.Size();

    SizeType totalCount = numPermutations;

    for (const ShaderProperty& valueGroup : valueGroups)
    {
        totalCount += valueGroup.enumValues.Size() * totalCount;
    }

    Array<ShaderProperties> allCombinations;
    allCombinations.Reserve(totalCount);

    for (SizeType i = 0; i < numPermutations; i++)
    {
        HashSet<ShaderProperty> currentProperties;
        currentProperties.Reserve(ByteUtil::BitCount(i) + staticProperties.Size());
        currentProperties.Merge(staticProperties);

        for (SizeType j = 0; j < variableProperties.Size(); j++)
        {
            if (i & (1ull << j))
            {
                currentProperties.Insert(variableProperties[j]);
            }
        }

        allCombinations.EmplaceBack(std::move(currentProperties));
    }

    HYP_LOG(
        ShaderCompiler,
        Debug,
        "Shader value groups: {}",
        valueGroups.Size());

    // now apply the value groups onto it
    for (const ShaderProperty& valueGroup : valueGroups)
    {
        // each value group causes the # of combinations to grow by (num values in group) * (current number of combinations)
        Array<ShaderProperties> currentGroupCombinations;
        currentGroupCombinations.Resize(valueGroup.enumValues.Size() * allCombinations.Size());

        for (SizeType existingCombinationIndex = 0; existingCombinationIndex < allCombinations.Size(); existingCombinationIndex++)
        {
            for (SizeType valueIndex = 0; valueIndex < valueGroup.enumValues.Size(); valueIndex++)
            {
                ShaderProperty newProperty(NAME_FMT("{}_{}", valueGroup.name, valueGroup.enumValues[valueIndex]), false);

                // copy the current version of the array
                ShaderProperties mergedProperties = allCombinations[existingCombinationIndex];
                mergedProperties.Set(newProperty);

                currentGroupCombinations[existingCombinationIndex + (valueIndex * allCombinations.Size())] = std::move(mergedProperties);
            }
        }

        HYP_LOG(
            ShaderCompiler,
            Debug,
            "\tShader value group {} has {} combinations:",
            valueGroup.name,
            currentGroupCombinations.Size());

        for (const ShaderProperties& properties : currentGroupCombinations)
        {
            HYP_LOG(ShaderCompiler, Debug, "\t\t{}", String::Join(properties, ", ", [](auto&& elem)
                                                         {
                                                             return String(*elem.GetName());
                                                         }));
        }

        allCombinations.Concat(std::move(currentGroupCombinations));
    }

    // #ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(
        ShaderCompiler,
        Debug,
        "Processing {} shader combinations:",
        allCombinations.Size());
    // #endif

    if (parallel)
    {
        TaskSystem::GetInstance().ParallelForEach(
            allCombinations,
            [&callback](const ShaderProperties& properties, uint32, uint32)
            {
                callback(properties);
            });
    }
    else
    {
        for (const ShaderProperties& properties : allCombinations)
        {
            callback(properties);
        }
    }
}

static bool LoadBatchFromFile(const FilePath& filepath, CompiledShaderBatch& outBatch)
{
    // read file if it already exists.
    FBOMReader reader { FBOMReaderConfig {} };

    FBOMResult err;

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

    Optional<CompiledShaderBatch&> compiledShaderBatchOpt = value.TryGet<CompiledShaderBatch>();

    if (!compiledShaderBatchOpt.HasValue())
    {
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Failed to load compiled shader at path: {}\n\tMessage: {}",
            filepath,
            "Failed to deserialize CompiledShaderBatch");

        return false;
    }

    outBatch = *compiledShaderBatchOpt;

    return true;
}

#pragma region ShaderProperty

String ShaderProperty::GetValueString() const
{
    if (HasValue())
    {
        String str;

        Visit(currentValue, [&str](auto&& value)
            {
                str = HYP_FORMAT("{}", value);
            });

        return str;
    }

    return String::empty;
}


#pragma endregion ShaderProperty

#pragma region ShaderProperties

ShaderProperties& ShaderProperties::Set(const ShaderProperty& property, bool enabled)
{
    if (property.IsVertexAttribute())
    {
        VertexAttribute::Type type;

        if (!FindVertexAttributeForDefinition(property.GetValueString(), type))
        {
            HYP_LOG(ShaderCompiler, Error, "Invalid vertex attribute name for shader: {}", property.GetValueString());

            return *this;
        }

        if (property.IsOptionalVertexAttribute())
        {
            if (enabled)
            {
                m_optionalVertexAttributes |= type;
                m_optionalVertexAttributes &= ~m_requiredVertexAttributes;
            }
            else
            {
                m_optionalVertexAttributes &= ~type;
            }

            // NOTE: Optional vertex attributes should not trigger any hash code recalculation.

            return *this;
        }

        if (enabled)
        {
            m_requiredVertexAttributes |= type;
            m_optionalVertexAttributes &= ~type;
        }
        else
        {
            m_requiredVertexAttributes &= ~type;
        }

        m_needsHashCodeRecalculation = true;

        return *this;
    }

    const auto it = m_props.Find(property);

    if (enabled)
    {
        if (it == m_props.End())
        {
            m_props.Insert(property);

            m_needsHashCodeRecalculation = true;

            return *this;
        }

        if (*it != property)
        {
            *it = property;

            m_needsHashCodeRecalculation = true;
        }

        return *this;
    }

    if (it != m_props.End())
    {
        m_props.Erase(it);

        m_needsHashCodeRecalculation = true;
    }

    return *this;
}

String ShaderProperties::ToString(bool includeVertexAttributes) const
{
    Array<String> propertyNames;
    propertyNames.Reserve((includeVertexAttributes ? ByteUtil::BitCount(GetRequiredVertexAttributes().GetFlagMask()) : 0) + GetPropertySet().Size());

    if (includeVertexAttributes)
    {
        for (Bitset::BitIndex i : Bitset(GetRequiredVertexAttributes().GetFlagMask()))
        {
            AssertDebug(int(i) < int(VertexAttribute::mapping.Size()));
            AssertDebug(VertexAttribute::mapping.ValueAt(i).name != nullptr, "Vertex attribute name missing at index {}, flagmask: {}", i, GetRequiredVertexAttributes().GetFlagMask());

            propertyNames.PushBack(String("HYP_ATTRIBUTE_") + VertexAttribute::mapping.ValueAt(i).name);
        }
    }

    for (const ShaderProperty& property : GetPropertySet())
    {
        propertyNames.PushBack(property.name.LookupString());
    }

    String propertiesString;
    SizeType index = 0;

    for (const String& name : propertyNames)
    {
        propertiesString += name;

        if (index != propertyNames.Size() - 1)
        {
            propertiesString += ", ";
        }

        index++;
    }

    return propertiesString;
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
    properties.Set(ShaderProperty(NAME("HYP_VULKAN"), false));

    constexpr uint32 vulkanVersion = HYP_VULKAN_API_VERSION;

    switch (vulkanVersion)
    {
    case VK_API_VERSION_1_1:
        properties.Set(ShaderProperty(NAME("HYP_VULKAN_1_1"), false));
        break;
    case VK_API_VERSION_1_2:
        properties.Set(ShaderProperty(NAME("HYP_VULKAN_1_2"), false));
        break;
#ifdef VK_API_VERSION_1_3
    case VK_API_VERSION_1_3:
        properties.Set(ShaderProperty(NAME("HYP_VULKAN_1_3"), false));
        break;
#endif
    default:
        break;
    }
#endif

#if defined(HYP_WINDOWS)
    properties.Set(ShaderProperty(NAME("HYP_WINDOWS"), false));
#elif defined(HYP_LINUX)
    properties.Set(ShaderProperty(NAME("HYP_LINUX"), false));
#elif defined(HYP_MACOS)
    properties.Set(ShaderProperty(NAME("HYP_MACOS"), false));
#elif defined(HYP_IOS)
    properties.Set(ShaderProperty(NAME("HYP_IOS"), false));
#endif

    properties.Set(ShaderProperty(NAME("NUM_GBUFFER_TEXTURES"), false, ShaderProperty::Value(int(g_numGbufferTargets))));

    if (g_renderBackend->GetRenderConfig().IsDynamicDescriptorIndexingSupported())
    {
        properties.Set(ShaderProperty(NAME("HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING"), false));
    }

    if (g_renderBackend->GetRenderConfig().IsBindlessSupported())
    {
        properties.Set(ShaderProperty(NAME("HYP_FEATURES_BINDLESS_TEXTURES"), false));
    }

    if (!g_renderBackend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial())
    {
        properties.Set(ShaderProperty(NAME("HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA"), false));
    }

    // props.Set(ShaderProperty("HYP_MAX_SHADOW_MAPS", false));
    // props.Set(ShaderProperty("HYP_MAX_BONES", false));
}

void ShaderCompiler::ParseDefinitionSection(const INIFile::Section& section, ShaderCompiler::Bundle& bundle)
{
    for (const auto& sectionIt : section)
    {
        if (sectionIt.first == "permute")
        {
            // set each property
            for (const auto& element : sectionIt.second.elements)
            {
                if (element.subElements.Any())
                {
                    bundle.versions.AddValueGroup(CreateNameFromDynamicString(*element.name), element.subElements);
                }
                else
                {
                    bundle.versions.AddPermutation(CreateNameFromDynamicString(*element.name));
                }
            }
        }
        else if (sectionIt.first == "entry_point")
        {
            bundle.entryPointName = sectionIt.second.GetValue().name;
        }
        else if (shaderTypeNames.Contains(sectionIt.first))
        {
            bundle.sources[shaderTypeNames.At(sectionIt.first)] = SourceFile {
                GetResourceDirectory() / "shaders" / sectionIt.second.GetValue().name
            };
        }
        else
        {
            HYP_LOG(
                ShaderCompiler,
                Warning,
                "Unknown property in shader definition file: {}\n",
                sectionIt.first);
        }
    }
}

bool ShaderCompiler::HandleCompiledShaderBatch(
    Bundle& bundle,
    const ShaderProperties& requestedProperties,
    const FilePath& outputFilePath,
    CompiledShaderBatch& batch)
{
    // Check that each version specified is present in the CompiledShaderBatch.
    // OR any src files have been changed since the object file was compiled.
    // if not, we need to recompile those versions.

    // TODO: Each individual item should have a timestamp internally set
    const Time objectFileLastModified = outputFilePath.LastModifiedTimestamp();

    Time maxSourceFileLastModified = Time(0);

    for (auto& sourceFile : bundle.sources)
    {
        maxSourceFileLastModified = MathUtil::Max(maxSourceFileLastModified, FilePath(sourceFile.second.path).LastModifiedTimestamp());
    }

    if (maxSourceFileLastModified > objectFileLastModified)
    {
        HYP_LOG(
            ShaderCompiler,
            Info,
            "Source file in batch {} has been modified since the batch was last compiled, recompiling...",
            *bundle.name);

        batch = CompiledShaderBatch {};

        return CompileBundle(bundle, requestedProperties, batch);
    }

    // find variants for the bundle that are not in the compiled batch
    Array<ShaderProperties> missingVariants;

    ForEachPermutation(bundle.versions, [&](const ShaderProperties& properties)
        {
            // get hashcode for this set of properties
            // only care about the property set (not vertex attributes), as we will only have access to those from the bundle
            // plus, changing vertex attributes will cause a recompile anyway due to shaders' file contents changing
            const HashCode propertiesHashCode = properties.GetPropertySetHashCode();

            const auto it = batch.compiledShaders.FindIf([propertiesHashCode](const CompiledShader& item)
                {
                    return item.GetDefinition().GetProperties().GetPropertySetHashCode() == propertiesHashCode;
                });

            if (it == batch.compiledShaders.End())
            {
                missingVariants.PushBack(properties);
            }
        },
        false);

    const bool requestedFound = batch.compiledShaders.FindIf([requestedPropertiesHashCode = requestedProperties.GetHashCode()](const CompiledShader& item)
                                    {
                                        return item.GetDefinition().GetProperties().GetHashCode() == requestedPropertiesHashCode;
                                    })
        != batch.compiledShaders.End();

    if (missingVariants.Any() || !requestedFound)
    {
        ShaderProperties allProperties;

        for (const CompiledShader& compiledShader : batch.compiledShaders)
        {
            //            HYP_LOG_TEMP("compiledShader flagmask : {}", compiledShader.GetRequiredVertexAttributes().GetFlagMask());

            allProperties.Merge(compiledShader.GetDefinition().GetProperties());
        }

        String missingVariantsString;

        {
            SizeType index = 0;

            for (const ShaderProperties& missingShaderProperties : missingVariants)
            {
                missingVariantsString += String::ToString(missingShaderProperties.GetHashCode().Value()) + " - " + missingShaderProperties.ToString();

                if (index != missingVariants.Size() - 1)
                {
                    missingVariantsString += ",\n\t";
                }

                index++;
            }
        }

        // clear the batch if properties requested are missing.
        batch = CompiledShaderBatch {};

        if (g_shouldCompileMissingVariants && CanCompileShaders())
        {
            HYP_LOG(
                ShaderCompiler,
                Info,
                "Compiled shader is missing properties. Attempting to compile with the missing properties.\n\tRequested with properties:\n\t{} ({})\n\n\tMissing variants:\n\t{}\n\n\tAll found properties: {}",
                requestedProperties.ToString(), (requestedFound ? "found" : "not found"),
                missingVariantsString,
                allProperties.ToString());

            return CompileBundle(bundle, requestedProperties, batch);
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

    const String nameString(name.LookupString());

    if (!m_definitions->HasSection(nameString))
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
    const INIFile::Section& section = m_definitions->GetSection(nameString);
    ParseDefinitionSection(section, bundle);

    auto tryToCompileOnMissing = [&](const FilePath& outputFilePath)
    {
        if (CanCompileShaders())
        {
            HYP_LOG(
                ShaderCompiler,
                Info,
                "Attempting to compile shader {}...",
                outputFilePath);
        }
        else
        {
            HYP_LOG(
                ShaderCompiler,
                Error,
                "Failed to load compiled shader file: {}. The file could not be found.",
                outputFilePath);

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

        return LoadBatchFromFile(outputFilePath, batch);
    };

    const FilePath outputFilePath = GetResourceDirectory() / "data/compiled_shaders" / nameString + ".hypshader";

    if (outputFilePath.Exists())
    {
        HYP_LOG(
            ShaderCompiler,
            Info,
            "Attempting to load compiled shader {}...",
            outputFilePath);

        if (!LoadBatchFromFile(outputFilePath, batch))
        {
            if (!tryToCompileOnMissing(outputFilePath))
            {
                return false;
            }
        }
    }
    else if (!tryToCompileOnMissing(outputFilePath))
    {
        return false;
    }

    return HandleCompiledShaderBatch(bundle, properties, outputFilePath, batch);
}

bool ShaderCompiler::LoadShaderDefinitions(bool precompileShaders)
{
    if (m_definitions && m_definitions->IsValid())
    {
        return true;
    }

    const FilePath dataPath = GetResourceDirectory() / "data/compiled_shaders";

    if (!dataPath.Exists())
    {
        if (FileSystem::MkDir(dataPath.Data()) != 0)
        {
            HYP_LOG(
                ShaderCompiler,
                Error,
                "Failed to create data path at {}",
                dataPath);

            return false;
        }
    }

    if (m_definitions)
    {
        delete m_definitions;
    }

    m_definitions = new INIFile(GetResourceDirectory() / "Shaders.ini");

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

    if (!precompileShaders)
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

        const Name nameFromString = CreateNameFromDynamicString(ANSIString(key));

        Bundle bundle { nameFromString };

        ParseDefinitionSection(section, bundle);

        bundles.PushBack(std::move(bundle));
    }

    const bool supportsRtShaders = g_renderBackend->GetRenderConfig().IsRaytracingSupported();

    HashMap<Bundle*, bool> results;
    Mutex resultsMutex;

    // Compile all shaders ahead of time
    TaskSystem::GetInstance().ParallelForEach(
        bundles,
        [&](auto& bundle, uint32, uint32)
        {
            if (bundle.HasRTShaders() && !supportsRtShaders)
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
                bundle.versions.Merge(ShaderProperties(staticMeshVertexAttributes));
                bundle.versions.Merge(ShaderProperties(staticMeshVertexAttributes | skeletonVertexAttributes));
            }

            ForEachPermutation(bundle.versions, [&](const ShaderProperties& properties)
                {
                    CompiledShader compiledShader;
                    bool result = GetCompiledShader(bundle.name, properties, compiledShader);

                    Mutex::Guard guard(resultsMutex);
                    results[&bundle] = result;
                },
                false); // true);
        });

    bool allResults = true;

    for (const auto& it : results)
    {
        if (!it.second)
        {
            String permutationString;

            HYP_LOG(
                ShaderCompiler,
                Error,
                "{}: Loading of compiled shader failed with version hash {}",
                it.first->name,
                it.first->versions.GetHashCode().Value());

            allResults = false;
        }
    }

    return allResults;
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
        String preprocessedSource;

        Array<String> preprocessErrorMessages;
        const bool preprocessResult = PreprocessShaderSource(type, language, BuildPreamble(properties), source, filename, preprocessedSource, preprocessErrorMessages);

        result.errors.Concat(Map(preprocessErrorMessages, [](const String& errorMessage)
            {
                return ProcessError { errorMessage };
            }));

        if (!preprocessResult)
        {
            return result;
        }

        lines = preprocessedSource.Split('\n');
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

    auto parseCustomStatement = [](const String& start, const String& line) -> ParseCustomStatementResult
    {
        const String substr = line.Substr(start.Length());

        String argsString;

        int parenthesesDepth = 0;
        SizeType index;

        // Note: using 'Size' and 'Data' to index -- not using utf-8 chars here.
        for (index = 0; index < substr.Size(); index++)
        {
            if (substr.Data()[index] == ')')
            {
                parenthesesDepth--;
            }

            if (parenthesesDepth > 0)
            {
                argsString.Append(substr.Data()[index]);
            }

            if (substr.Data()[index] == '(')
            {
                parenthesesDepth++;
            }

            if (parenthesesDepth <= 0)
            {
                break;
            }
        }

        Array<String> args = argsString.Split(',');

        for (String& arg : args)
        {
            arg = arg.Trimmed();
        }

        return {
            std::move(args),
            substr.Substr(index + 1)
        };
    };

    int lastAttributeLocation = -1;

    for (uint32 lineIndex = 0; lineIndex < lines.Size();)
    {
        const String line = lines[lineIndex++].Trimmed();

        switch (phase)
        {
        case ProcessShaderSourcePhase::BEFORE_PREPROCESS:
        {
            if (line.StartsWith("HYP_ATTRIBUTE"))
            {
                Array<String> parts = line.Split(' ');

                bool optional = false;

                // if (parts.Size() != 3)
                // {
                //     result.errors.PushBack(ProcessError { "Invalid attribute: Requires format HYP_ATTRIBUTE(location) type name" });

                //     break;
                // }

                char ch;

                String attributeKeyword,
                    attributeType,
                    attributeLocation,
                    attributeName;

                Optional<String> attributeCondition;

                {
                    SizeType index = 0;

                    while (index != parts.Front().Size() && (std::isalpha(ch = parts.Front()[index]) || ch == '_'))
                    {
                        attributeKeyword.Append(ch);
                        ++index;
                    }

                    if (attributeKeyword == "HYP_ATTRIBUTE_OPTIONAL")
                    {
                        optional = true;
                    }
                    else if (attributeKeyword == "HYP_ATTRIBUTE")
                    {
                        optional = false;
                    }
                    else
                    {
                        result.errors.PushBack(ProcessError { String("Invalid attribute, unknown attribute keyword `") + attributeKeyword + "`" });

                        break;
                    }

                    if (index != parts.Front().Size() && ((ch = parts.Front()[index]) == '('))
                    {
                        ++index;

                        // read integer string
                        while (index != parts.Front().Size() && std::isdigit(ch = parts.Front()[index]))
                        {
                            attributeLocation.Append(ch);
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

                            attributeCondition = condition;
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

                        if (attributeLocation.Empty())
                        {
                            result.errors.PushBack(ProcessError { "Invalid attribute location" });

                            break;
                        }
                    }
                }

                for (SizeType index = 0; index < parts[1].Size() && (std::isalnum(ch = parts[1][index]) || ch == '_'); index++)
                {
                    attributeType.Append(ch);
                }

                for (SizeType index = 0; index < parts[2].Size() && (std::isalnum(ch = parts[2][index]) || ch == '_'); index++)
                {
                    attributeName.Append(ch);
                }

                VertexAttributeDefinition attributeDefinition;
                attributeDefinition.name = attributeName;
                attributeDefinition.typeClass = attributeType;
                attributeDefinition.location = attributeLocation.Any()
                    ? std::atoi(attributeLocation.Data())
                    : lastAttributeLocation + 1;

                lastAttributeLocation = attributeDefinition.location;

                if (optional)
                {
                    result.optionalAttributes.PushBack(attributeDefinition);

                    if (attributeCondition.HasValue())
                    {
                        result.processedSource += "#if defined(" + attributeCondition.Get() + ") && " + attributeCondition.Get() + "\n";

                        attributeDefinition.condition = attributeCondition;
                    }
                    else
                    {
                        result.processedSource += "#ifdef HYP_ATTRIBUTE_" + attributeDefinition.name + "\n";
                    }
                }
                else
                {
                    result.requiredAttributes.PushBack(attributeDefinition);
                }

                result.processedSource += "layout(location=" + String::ToString(attributeDefinition.location) + ") in " + attributeDefinition.typeClass + " " + attributeDefinition.name + ";\n";

                if (optional)
                {
                    result.processedSource += "#endif\n";
                }

                continue;
            }

            break;
        }
        case ProcessShaderSourcePhase::AFTER_PREPROCESS:
        {
            if (line.StartsWith("HYP_DESCRIPTOR"))
            {
                String commandStr;

                for (SizeType index = 0; index < line.Size(); index++)
                {
                    if (std::isalnum(line.Data()[index]) || line.Data()[index] == '_')
                    {
                        commandStr.Append(line.Data()[index]);
                    }
                    else
                    {
                        break;
                    }
                }

                DescriptorSlot slot = DESCRIPTOR_SLOT_NONE;
                DescriptorUsageFlags flags = DESCRIPTOR_USAGE_FLAG_NONE;

                if (commandStr == "HYP_DESCRIPTOR_SRV")
                {
                    slot = DESCRIPTOR_SLOT_SRV;
                }
                else if (commandStr == "HYP_DESCRIPTOR_UAV")
                {
                    slot = DESCRIPTOR_SLOT_UAV;
                }
                else if (commandStr == "HYP_DESCRIPTOR_CBUFF" || commandStr == "HYP_DESCRIPTOR_CBUFF_DYNAMIC")
                {
                    slot = DESCRIPTOR_SLOT_CBUFF;

                    if (commandStr == "HYP_DESCRIPTOR_CBUFF_DYNAMIC")
                    {
                        flags |= DESCRIPTOR_USAGE_FLAG_DYNAMIC;
                    }
                }
                else if (commandStr == "HYP_DESCRIPTOR_SSBO" || commandStr == "HYP_DESCRIPTOR_SSBO_DYNAMIC")
                {
                    slot = DESCRIPTOR_SLOT_SSBO;

                    if (commandStr == "HYP_DESCRIPTOR_SSBO_DYNAMIC")
                    {
                        flags |= DESCRIPTOR_USAGE_FLAG_DYNAMIC;
                    }
                }
                else if (commandStr == "HYP_DESCRIPTOR_ACCELERATION_STRUCTURE")
                {
                    slot = DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE;
                }
                else if (commandStr == "HYP_DESCRIPTOR_SAMPLER")
                {
                    slot = DESCRIPTOR_SLOT_SAMPLER;
                }
                else
                {
                    result.errors.PushBack(ProcessError { "Invalid descriptor slot. Must match HYP_DESCRIPTOR_<Type> " });

                    break;
                }

                Array<String> parts = line.Split(' ');

                String descriptorName,
                    setName,
                    slotStr;

                HashMap<String, String> params;

                auto parseResult = parseCustomStatement(commandStr, line);

                if (parseResult.args.Size() < 2)
                {
                    result.errors.PushBack(ProcessError { "Invalid descriptor: Requires format HYP_DESCRIPTOR_<TYPE>(set, name)" });

                    break;
                }

                setName = parseResult.args[0];
                descriptorName = parseResult.args[1];

                if (parseResult.args.Size() > 2)
                {
                    for (SizeType index = 2; index < parseResult.args.Size(); index++)
                    {
                        Array<String> split = parseResult.args[index].Split('=');

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
                    CreateNameFromDynamicString(ANSIString(setName)),
                    CreateNameFromDynamicString(ANSIString(descriptorName)),
                    flags,
                    std::move(params)
                };

                String stdVersion = "std140";

                if (usage.params.Contains("standard"))
                {
                    stdVersion = usage.params.At("standard");
                }

                Array<String> additionalParams;

                if (usage.params.Contains("format"))
                {
                    additionalParams.PushBack(usage.params.At("format"));
                }

                // add std140, std430 etc. for buffer types.
                switch (usage.slot)
                {
                case DESCRIPTOR_SLOT_CBUFF: // fallthrough
                case DESCRIPTOR_SLOT_SSBO:  // fallthrough
                    if (usage.params.Contains("matrix_mode"))
                    {
                        additionalParams.PushBack(usage.params.At("matrix_mode"));
                    }
                    else
                    {
                        additionalParams.PushBack("row_major");
                    }

                    result.processedSource += "layout(" + stdVersion + ", set=HYP_DESCRIPTOR_SET_INDEX_" + setName + ", binding=HYP_DESCRIPTOR_INDEX_" + setName + "_" + descriptorName + (additionalParams.Any() ? (", " + String::Join(additionalParams, ", ")) : String::empty) + ") " + parseResult.remaining + "\n";
                    break;
                default:
                    result.processedSource += "layout(set=HYP_DESCRIPTOR_SET_INDEX_" + setName + ", binding=HYP_DESCRIPTOR_INDEX_" + setName + "_" + descriptorName + (additionalParams.Any() ? (", " + String::Join(additionalParams, ", ")) : String::empty) + ") " + parseResult.remaining + "\n";
                    break;
                }

                result.descriptorUsages.PushBack(usage);

                continue;
            }

            break;
        }
        }

        result.processedSource += line + '\n';
    }

#ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(ShaderCompiler, Info, "Processed source: {}", result.processedSource);
#endif

    return result;
}

bool ShaderCompiler::CompileBundle(
    Bundle& bundle,
    const ShaderProperties& additionalVersions,
    CompiledShaderBatch& out)
{
    if (!CanCompileShaders())
    {
        return false;
    }

    // run with spirv-cross
    FileSystem::MkDir((GetResourceDirectory() / "data/compiled_shaders/tmp").Data());

    Array<LoadedSourceFile> loadedSourceFiles;
    loadedSourceFiles.Resize(bundle.sources.Size());

    Array<Array<ProcessError>> processErrors;
    processErrors.Resize(bundle.sources.Size());

    Array<Array<VertexAttributeDefinition>> requiredVertexAttributes;
    requiredVertexAttributes.Resize(bundle.sources.Size());

    Array<Array<VertexAttributeDefinition>> optionalVertexAttributes;
    optionalVertexAttributes.Resize(bundle.sources.Size());

    TaskBatch taskBatch;

    for (SizeType index = 0; index < bundle.sources.Size(); index++)
    {
        StaticMessage debugName;
        debugName.value = ANSIStringView(bundle.sources.AtIndex(index).second.path.Data());

        taskBatch.AddTask([this, index, &bundle, &loadedSourceFiles, &processErrors, &requiredVertexAttributes, &optionalVertexAttributes](...)
            {
                const auto& it = bundle.sources.AtIndex(index);

                const FilePath filepath = it.second.path;
                const ShaderLanguage language = filepath.EndsWith("hlsl") ? ShaderLanguage::HLSL : ShaderLanguage::GLSL;

                if (!filepath.Exists())
                {
                    // file does not exist!
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "Shader source file does not exist at {}",
                        filepath);

                    processErrors[index] = {
                        ProcessError { "Shader source file does not exist" }
                    };

                    return;
                }

                FileBufferedReaderSource filepathSource { filepath };
                BufferedReader reader { &filepathSource };

                if (!reader.IsOpen())
                {
                    // could not open file!
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "Failed to open shader source file at {}",
                        filepath);

                    processErrors[index] = {
                        ProcessError { "Failed to open source file" }
                    };

                    return;
                }

                const ByteBuffer byteBuffer = reader.ReadBytes();
                String sourceString(byteBuffer.ToByteView());

                // process shader source to extract vertex attributes.
                // runs before actual preprocessing
                ProcessResult result = ProcessShaderSource(ProcessShaderSourcePhase::BEFORE_PREPROCESS, it.first, language, sourceString, filepath, {});

                if (result.errors.Any())
                {
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "{} shader processing errors:",
                        result.errors.Size());

                    processErrors[index] = result.errors;

                    return;
                }

                requiredVertexAttributes[index] = result.requiredAttributes;
                optionalVertexAttributes[index] = result.optionalAttributes;

                loadedSourceFiles[index] = LoadedSourceFile {
                    .type = it.first,
                    .language = language,
                    .file = it.second,
                    .lastModifiedTimestamp = filepath.LastModifiedTimestamp(),
                    .source = result.processedSource
                };
            });
    }

    if (Threads::IsOnThread(ThreadCategory::THREAD_CATEGORY_TASK))
    {
        // run on this thread if we are already in a task thread
        taskBatch.ExecuteBlocking();
    }
    else
    {
        // Hack fix: task threads that are currently enqueueing RenderCommands can cause a deadlock,
        // if we are waiting on tasks to complete from the render thread.

        if (Threads::IsOnThread(g_renderThread))
        {
            taskBatch.ExecuteBlocking();
        }
        else
        {
            TaskSystem::GetInstance().EnqueueBatch(&taskBatch);
            taskBatch.AwaitCompletion();
        }
    }

    Array<ProcessError> allProcessErrors;

    for (const auto& errorList : processErrors)
    {
        allProcessErrors.Concat(errorList);
    }

    if (!allProcessErrors.Empty())
    {
        for (const ProcessError& error : allProcessErrors)
        {
            HYP_LOG(
                ShaderCompiler,
                Error,
                "\t{}",
                error.errorMessage);
        }

        return false;
    }

    // // Merge all descriptor usages together
    // for (const Array<DescriptorUsage> &descriptorUsageList : descriptorUsages) {
    //     bundle.descriptorUsages.Merge(descriptorUsageList);
    // }

    // grab each defined property, and iterate over each combination
    ShaderProperties finalProperties;
    finalProperties.Merge(bundle.versions);

    { // Lookup vertex attribute names

        VertexAttributeSet requiredVertexAttributeSet;
        VertexAttributeSet optionalVertexAttributeSet;

        for (const auto& definitions : requiredVertexAttributes)
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

                requiredVertexAttributeSet |= type;
            }
        }

        for (const auto& definitions : optionalVertexAttributes)
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

                optionalVertexAttributeSet |= type;
            }
        }

        finalProperties.SetRequiredVertexAttributes(requiredVertexAttributeSet);
        finalProperties.SetOptionalVertexAttributes(optionalVertexAttributeSet);
    }

    // copy passed properties
    finalProperties.Merge(additionalVersions);

    HYP_LOG(
        ShaderCompiler,
        Info,
        "Compiling shader bundle for shader {}",
        bundle.name);

    // update versions to include vertex attribute properties
    bundle.versions = finalProperties;

    Mutex fsMutex;
    Mutex compiledShadersMutex;
    Mutex errorMessagesMutex;

    AtomicVar<uint32> numCompiledPermutations { 0u };
    AtomicVar<uint32> numErroredPermutations { 0u };

    auto buildVertexAttributeSet = [](const Array<VertexAttributeDefinition>& definitions) -> VertexAttributeSet
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
    ForEachPermutation(finalProperties, [&](const ShaderProperties& properties)
        {
            CompiledShader compiledShader;

            compiledShader.definition = ShaderDefinition {
                bundle.name,
                properties
            };

            compiledShader.entryPointName = bundle.entryPointName;

            Assert(compiledShader.definition.IsValid());

            AtomicVar<bool> anyFilesCompiled { false };
            AtomicVar<bool> anyFilesErrored { false };

            Array<DescriptorUsageSet> descriptorUsageSetsPerFile;
            descriptorUsageSetsPerFile.Resize(loadedSourceFiles.Size());

            Array<String> processedSources;
            processedSources.Resize(loadedSourceFiles.Size());

            Array<Pair<FilePath, bool /* skip */>> filepaths;
            filepaths.Resize(loadedSourceFiles.Size());

            // load each source file, check if the output file exists, and if it does, check if it is older than the source file
            // if it is, we can reuse the file.
            // otherwise, we process the source and prepare it for compilation
            for (SizeType index = 0; index < loadedSourceFiles.Size(); index++)
            {
                const LoadedSourceFile& item = loadedSourceFiles[index];

                // check if a file exists w/ same hash
                const FilePath outputFilepath = item.GetOutputFilepath(
                    GetResourceDirectory(),
                    compiledShader.definition);

                filepaths[index] = { outputFilepath, false };

                DescriptorUsageSet& descriptorUsages = descriptorUsageSetsPerFile[index];

                Array<String> errorMessages;

                // set directory to the directory of the shader
                const FilePath dir = GetResourceDirectory() / FilePath::Relative(FilePath(item.file.path).BasePath(), GetResourceDirectory());

                String& processedSource = processedSources[index];

                { // Process shader (preprocessing, custom statements, etc.)
                    ProcessResult processResult = ProcessShaderSource(ProcessShaderSourcePhase::AFTER_PREPROCESS, item.type, item.language, item.source, item.file.path, properties);

                    if (processResult.errors.Any())
                    {
                        HYP_LOG(
                            ShaderCompiler,
                            Error,
                            "{} shader processing errors:",
                            processResult.errors.Size());

                        Mutex::Guard guard(errorMessagesMutex);

                        out.errorMessages.Concat(Map(processResult.errors, &ProcessError::errorMessage));

                        anyFilesErrored.Set(true, MemoryOrder::RELAXED);

                        return;
                    }

                    descriptorUsages.Merge(std::move(processResult.descriptorUsages));

                    processedSource = processResult.processedSource;
                }
            }

            // merge all descriptor usages together for the source files before compiling.
            DescriptorUsageSet descriptorUsageSetsMerged;

            for (const DescriptorUsageSet& descriptorUsageSet : descriptorUsageSetsPerFile)
            {
                descriptorUsageSetsMerged.Merge(descriptorUsageSet);
            }

            descriptorUsageSetsPerFile.Clear();

            // final substitution of properties + compilation
            for (SizeType index = 0; index < loadedSourceFiles.Size(); index++)
            {
                const LoadedSourceFile& item = loadedSourceFiles[index];

                const Pair<FilePath, bool>& filepathState = filepaths[index];

                // don't process these files
                if (filepathState.second)
                {
                    return;
                }

                const FilePath& outputFilepath = filepathState.first;

                Array<String> errorMessages;

                { // logging stuff
                    String variablePropertiesString;
                    String staticPropertiesString;

                    for (const ShaderProperty& property : properties.ToArray())
                    {
                        if (property.isPermutation)
                        {
                            if (!variablePropertiesString.Empty())
                            {
                                variablePropertiesString += ", ";
                            }

                            variablePropertiesString += property.name.LookupString();
                        }
                        else
                        {
                            if (!staticPropertiesString.Empty())
                            {
                                staticPropertiesString += ", ";
                            }

                            staticPropertiesString += property.name.LookupString();
                        }
                    }

                    // HYP_LOG(
                    //     ShaderCompiler,
                    //     Info,
                    //     "Compiling shader {}\n\tVariable properties: [{}]\n\tStatic properties: [{}]\n\tProperties hash: {}",
                    //     outputFilepath,
                    //     variablePropertiesString,
                    //     staticPropertiesString,
                    //     properties.GetHashCode().Value()
                    // );
                }

                ByteBuffer byteBuffer = CompileToSPIRV(item.type, item.language, descriptorUsageSetsMerged, processedSources[index], item.file.path, properties, errorMessages);

                if (byteBuffer.Empty())
                {
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "Failed to compile file {} with version hash {}",
                        item.file.path,
                        properties.GetHashCode().Value());

                    Mutex::Guard guard(errorMessagesMutex);
                    out.errorMessages.Concat(std::move(errorMessages));

                    anyFilesErrored.Set(true, MemoryOrder::RELAXED);

                    return;
                }

                compiledShader.descriptorUsageSet = descriptorUsageSetsMerged;

                // write the spirv to the output file
                FileByteWriter spirvWriter(outputFilepath.Data());

                if (!spirvWriter.IsOpen())
                {
                    HYP_LOG(
                        ShaderCompiler,
                        Error,
                        "Could not open file {} for writing!",
                        outputFilepath);

                    anyFilesErrored.Set(true, MemoryOrder::RELAXED);

                    return;
                }

                spirvWriter.Write(byteBuffer.Data(), byteBuffer.Size());
                spirvWriter.Close();

                anyFilesCompiled.Set(true, MemoryOrder::RELAXED);

                compiledShader.modules[item.type] = std::move(byteBuffer);
            }

            numCompiledPermutations.Increment(uint32(!anyFilesErrored.Get(MemoryOrder::RELAXED) && anyFilesCompiled.Get(MemoryOrder::RELAXED)), MemoryOrder::RELAXED);
            numErroredPermutations.Increment(uint32(anyFilesErrored.Get(MemoryOrder::RELAXED)), MemoryOrder::RELAXED);

            compiledShader.descriptorTableDeclaration = compiledShader.descriptorUsageSet.BuildDescriptorTableDeclaration();

            Mutex::Guard guard(compiledShadersMutex);
            out.compiledShaders.PushBack(std::move(compiledShader));
        },
        false); // true);

    if (numErroredPermutations.Get(MemoryOrder::RELAXED))
    {
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Failed to compile {} permutations of shader {}",
            numErroredPermutations.Get(MemoryOrder::RELAXED),
            bundle.name);

        return false;
    }

    const FilePath finalOutputPath = GetResourceDirectory() / "data/compiled_shaders" / String(bundle.name.LookupString()) + ".hypshader";

    FileByteWriter byteWriter(finalOutputPath.Data());

    FBOMWriter serializer { FBOMWriterConfig {} };

    if (FBOMResult err = serializer.Append(out))
    {
        HYP_BREAKPOINT_DEBUG_MODE;

        return false;
    }

    if (FBOMResult err = serializer.Emit(&byteWriter))
    {
        HYP_BREAKPOINT_DEBUG_MODE;

        return false;
    }

    byteWriter.Close();

    m_cache.Set(bundle.name, out);

    if (numCompiledPermutations.Get(MemoryOrder::RELAXED) != 0)
    {
        HYP_LOG(
            ShaderCompiler,
            Info,
            "Compiled {} new variants for shader {} to: {}",
            numCompiledPermutations.Get(MemoryOrder::RELAXED),
            bundle.name,
            finalOutputPath);
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
    CompiledShader compiledShader;

    GetCompiledShader(
        name,
        properties,
        compiledShader);

    return compiledShader;
}

bool ShaderCompiler::GetCompiledShader(
    Name name,
    const ShaderProperties& properties,
    CompiledShader& out)
{
    ShaderProperties finalProperties;
    GetPlatformSpecificProperties(finalProperties);
    finalProperties.Merge(properties);

    const HashCode finalPropertiesHash = finalProperties.GetHashCode();

    if (m_cache.GetShaderInstance(name, finalPropertiesHash.Value(), out))
    {
        return true;
    }

    CompiledShaderBatch batch;

    if (!LoadOrCompileBatch(name, finalProperties, batch))
    {
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Failed to attempt loading of shader batch: {}\n\tRequested instance with properties: [{}]",
            name,
            finalProperties.ToString());

        return false;
    }

    // set in cache so we can use it later
    m_cache.Set(name, batch);

    // make sure we properly created it

    auto it = batch.compiledShaders.FindIf([finalPropertiesHash](const CompiledShader& compiledShader)
        {
            if (!compiledShader.IsValid())
            {
                return false;
            }

            return compiledShader.GetDefinition().GetProperties().GetHashCode() == finalPropertiesHash;
        });

    if (it == batch.compiledShaders.End())
    {
        HYP_LOG(
            ShaderCompiler,
            Error,
            "Hash calculation for shader {} does not match {}! Invalid shader property combination.\n\tRequested instance with properties: [{}]\n\tFound property sets: {}",
            name,
            finalPropertiesHash.Value(),
            finalProperties.ToString(),
            String::Join(batch.compiledShaders, "\n\t", [](auto&& item)
                {
                    return HYP_FORMAT("Props: {}, Hash: {}", item.GetDefinition().GetProperties().ToString(), item.GetDefinition().GetProperties().GetHashCode().Value());
                }));

        HYP_BREAKPOINT;

        return false;
    }

    out = *it;

    HYP_LOG(
        ShaderCompiler,
        Debug,
        "Selected shader {} for hash {}.\n\tRequested instance with properties: [{}]",
        name,
        finalPropertiesHash.Value(),
        finalProperties.ToString());

    Assert(out.GetDefinition().IsValid());

    return true;
}

#pragma endregion ShaderCompiler

} // namespace hyperion

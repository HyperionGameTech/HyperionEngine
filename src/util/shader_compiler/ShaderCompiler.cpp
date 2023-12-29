#include <util/shader_compiler/ShaderCompiler.hpp>
#include <asset/ByteWriter.hpp>
#include <asset/BufferedByteReader.hpp>
#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/ShaderBundleMarshal.hpp>
#include <math/MathUtil.hpp>
#include <util/definitions/DefinitionsFile.hpp>
#include <util/fs/FsUtil.hpp>
#include <util/Defines.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <bitset>

#ifdef HYP_GLSLANG

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#endif

namespace hyperion::v2 {

static const Bool should_compile_missing_variants = true;

enum class ShaderLanguage
{
    GLSL,
    HLSL
};

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

static ByteBuffer CompileToSPIRV(
    ShaderModuleType type,
    ShaderLanguage language,
    String preamble,
    String source,
    String filename,
    const ShaderProperties &properties,
    Array<String> &error_messages
)
{
#define GLSL_ERROR(log_type, error_message, ...) \
    { \
        DebugLog(log_type, error_message, __VA_ARGS__); \
        CharArray char_array; \
        int n = std::snprintf(nullptr, 0, error_message, __VA_ARGS__); \
        if (n > 0) { \
            char_array.Resize(static_cast<SizeType>(n) + 1); \
            std::sprintf(char_array.Data(), error_message, __VA_ARGS__); \
        } \
        error_messages.PushBack(String(char_array)); \
    }

    auto default_resources = DefaultResources();

    glslang_stage_t stage;
    String stage_string;

    switch (type) {
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

    UInt vulkan_api_version = MathUtil::Max(HYP_VULKAN_API_VERSION, VK_API_VERSION_1_1);
    UInt spirv_api_version = GLSLANG_TARGET_SPV_1_2;
    UInt spirv_version = 450;

    // Maybe remove... Some platforms giving crash
    // when loading vk1.2 shaders. But we need it for raytracing.
    if (ShaderModule::IsRaytracingType(type)) {
        vulkan_api_version = MathUtil::Max(vulkan_api_version, VK_API_VERSION_1_2);
        spirv_api_version = MathUtil::Max(spirv_api_version, GLSLANG_TARGET_SPV_1_4);
        spirv_version = MathUtil::Max(spirv_version, 460);
    }

    const glslang_input_t input {
        .language = language == ShaderLanguage::HLSL ? GLSLANG_SOURCE_HLSL : GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = static_cast<glslang_target_client_version_t>(vulkan_api_version),
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = static_cast<glslang_target_language_version_t>(spirv_api_version),
        .code = source.Data(),
        .default_version = Int(spirv_version),
        .default_profile = GLSLANG_CORE_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = reinterpret_cast<const glslang_resource_t *>(&default_resources),
    };

    glslang_shader_t *shader = glslang_shader_create(&input);

    if (stage_string.Any()) {
        preamble += "\n#ifndef " + stage_string + "\n#define " + stage_string + "\n#endif\n";
    }

    glslang_shader_set_preamble(shader, preamble.Data());

    if (!glslang_shader_preprocess(shader, &input))	{
        GLSL_ERROR(LogType::Error, "GLSL preprocessing failed %s\n", filename.Data());
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_info_log(shader));
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_info_debug_log(shader));
        GLSL_ERROR(LogType::Error, "%s\n", input.code);
        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    if (!glslang_shader_parse(shader, &input)) {
        GLSL_ERROR(LogType::Error, "GLSL parsing failed %s\n", filename.Data());
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_info_log(shader));
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_info_debug_log(shader));
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_preprocessed_code(shader));
        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    glslang_program_t *program = glslang_program_create();
    glslang_program_add_shader(program, shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        GLSL_ERROR(LogType::Error, "GLSL linking failed %s %s\n", filename.Data(), source.Data());
        GLSL_ERROR(LogType::Error, "%s\n", glslang_program_get_info_log(program));
        GLSL_ERROR(LogType::Error, "%s\n", glslang_program_get_info_debug_log(program));
        glslang_program_delete(program);
        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    glslang_program_SPIRV_generate(program, stage);

    ByteBuffer shader_module(glslang_program_SPIRV_get_size(program) * sizeof(UInt32));
    glslang_program_SPIRV_get(program, reinterpret_cast<UInt32 *>(shader_module.Data()));

    const char *spirv_messages = glslang_program_SPIRV_get_messages(program);

    if (spirv_messages) {
        GLSL_ERROR(LogType::Error, "(%s) %s\b", filename.Data(), spirv_messages);
    }

    glslang_program_delete(program);
    glslang_shader_delete(shader);

#undef GLSL_ERROR

    return shader_module;
}

#else


static ByteBuffer CompileToSPIRV(
    ShaderModuleType type,
    ShaderLanguage language,
    String preamble,
    String source,
    String filename,
    const ShaderProperties &properties,
    Array<String> &error_messages
)
{
    return ByteBuffer();
}

#endif

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

static void ForEachPermutation(
    const ShaderProperties &versions,
    Proc<void, const ShaderProperties &> callback
)
{
    Array<ShaderProperty> variable_properties;
    Array<ShaderProperty> static_properties;
    Array<ShaderProperty> value_groups;

    for (SizeType i = 0; i < VertexAttribute::mapping.Size(); i++) {
        const auto &kv = VertexAttribute::mapping.KeyValueAt(i);

        if (!kv.second.name) {
            continue;
        }

        if (versions.HasRequiredVertexAttribute(kv.first)) {
            static_properties.PushBack(ShaderProperty(kv.first));
        } else if (versions.HasOptionalVertexAttribute(kv.first)) {
            variable_properties.PushBack(ShaderProperty(kv.first));
        }
    }

    for (auto &property : versions.GetPropertySet()) {
        if (property.IsValueGroup()) {
            value_groups.PushBack(property);
        } else if (property.is_permutation) {
            variable_properties.PushBack(property);
        } else {
            static_properties.PushBack(property);
        }
    }

    const SizeType num_permutations = 1ull << variable_properties.Size();

    SizeType total_count = num_permutations;

    for (const auto &value_group : value_groups) {
        total_count += value_group.possible_values.Size() * total_count;
    }

    Array<Array<ShaderProperty>> all_combinations;
    all_combinations.Reserve(total_count);

    for (SizeType i = 0; i < num_permutations; i++) {
        Array<ShaderProperty> current_properties;
        current_properties.Reserve(MathUtil::BitCount(i) + static_properties.Size());
        current_properties.Concat(static_properties);

        for (SizeType j = 0; j < variable_properties.Size(); j++) {
            if (i & (1ull << j)) {
                current_properties.PushBack(variable_properties[j]);
            }
        }

        all_combinations.PushBack(std::move(current_properties));
    }

    // now apply the value groups onto it
    for (const auto &value_group : value_groups) {
        // each value group causes the # of combinations to grow by (num values in group) * (current number of combinations)
        Array<Array<ShaderProperty>> current_group_combinations;
        current_group_combinations.Resize(value_group.possible_values.Size() * all_combinations.Size());

        for (SizeType existing_combination_index = 0; existing_combination_index < all_combinations.Size(); existing_combination_index++) {
            for (SizeType value_index = 0; value_index < value_group.possible_values.Size(); value_index++) {
                ShaderProperty new_property(value_group.name + "_" + value_group.possible_values[value_index], false);

                // copy the current version of the array
                Array<ShaderProperty> merged_properties(all_combinations[existing_combination_index]);
                merged_properties.PushBack(std::move(new_property));

                current_group_combinations[existing_combination_index + (value_index * all_combinations.Size())] = std::move(merged_properties);
            }
        }

        all_combinations.Concat(std::move(current_group_combinations));
    }

    AssertThrowMsg(all_combinations.Size() == total_count, "Math is incorrect");

    DebugLog(
        LogType::Debug,
        "Processing %llu shader combinations:\n",
        all_combinations.Size()
    );

    TaskSystem::GetInstance().ParallelForEach(
        all_combinations,
        [&callback](const Array<ShaderProperty> &properties, UInt index, UInt batch_index) {
            const ShaderProperties combination_properties(properties);

            DebugLog(
                LogType::Debug,
                "Processing combination #%u: %s\n",
                index,
                combination_properties.ToString().Data()
            );

            callback(combination_properties);
        }
    );
}

renderer::DescriptorTable DescriptorUsageSet::BuildDescriptorTable() const
{
    renderer::DescriptorTable table;

    for (const DescriptorUsage &descriptor_usage : descriptor_usages) {
        renderer::DescriptorSetDeclaration *descriptor_set_declaration = table.FindDescriptorSetDeclaration(descriptor_usage.set_name);

        // check if this descriptor set is defined in the static descriptor table
        // if it is, we can use those definitions
        // otherwise, it is a 'custom' descriptor set
        renderer::DescriptorSetDeclaration *static_descriptor_set_declaration = renderer::g_static_descriptor_table
            ->FindDescriptorSetDeclaration(descriptor_usage.set_name);

        if (static_descriptor_set_declaration != nullptr) {
            if (!descriptor_set_declaration) {
                table.AddDescriptorSet(*static_descriptor_set_declaration);
            }

            continue;
        }

        if (!descriptor_set_declaration) {
            descriptor_set_declaration = table.AddDescriptorSet(renderer::DescriptorSetDeclaration(table.GetDescriptorSetDeclarations().Size(), descriptor_usage.set_name));
        }

        renderer::DescriptorDeclaration *descriptor_declaration = descriptor_set_declaration
            ->FindDescriptorDeclaration(descriptor_usage.descriptor_name);

        if (descriptor_declaration != nullptr) {
            // Already exists, just update the slot
            descriptor_declaration->name = descriptor_usage.descriptor_name;
            descriptor_declaration->slot = descriptor_usage.slot;
            descriptor_declaration->is_dynamic = descriptor_usage.flags & DESCRIPTOR_USAGE_FLAG_DYNAMIC;
        } else {
            descriptor_set_declaration->AddDescriptor(descriptor_usage.slot, descriptor_usage.descriptor_name, descriptor_usage.flags & DESCRIPTOR_USAGE_FLAG_DYNAMIC);
        }
    }

    return table;
}

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

    if (m_definitions) {
        delete m_definitions;
    }
}

void ShaderCompiler::GetPlatformSpecificProperties(ShaderProperties &properties) const
{
#if defined(HYP_VULKAN) && HYP_VULKAN
    properties.Set(ShaderProperty("HYP_VULKAN", false));

    constexpr UInt vulkan_version = HYP_VULKAN_API_VERSION;
    
    switch (vulkan_version) {
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

    if (g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
        properties.Set(ShaderProperty("HYP_FEATURES_BINDLESS_TEXTURES", false));
    }

    if (use_indexed_array_for_object_data) {
        properties.Set(ShaderProperty("HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA", false));
    }

    //props.Set(ShaderProperty("HYP_MAX_SHADOW_MAPS", false));
    //props.Set(ShaderProperty("HYP_MAX_BONES", false));
}

void ShaderCompiler::ParseDefinitionSection(
    const DefinitionsFile::Section &section,
    ShaderCompiler::Bundle &bundle
)
{
    for (const auto &section_it : section) {
        if (section_it.key == "permute") {
            // set each property
            for (const auto &element : section_it.value.elements) {
                if (element.sub_elements.Any()) {
                    bundle.versions.AddValueGroup(element.name, element.sub_elements);
                } else {
                    bundle.versions.AddPermutation(element.name);
                }
            }
        } else if (section_it.key == "entry_point") {
            bundle.entry_point_name = section_it.value.GetValue().name;
        } else if (shader_type_names.Contains(section_it.key)) {
            bundle.sources[shader_type_names.At(section_it.key)] = SourceFile {
                g_asset_manager->GetBasePath() / "shaders" / section_it.value.GetValue().name
            };
        } else {
            DebugLog(
                LogType::Warn,
                "Unknown property in shader definition file: %s\n",
                section_it.key.Data()
            );
        }
    }
}

Bool ShaderCompiler::HandleCompiledShaderBatch(
    Bundle &bundle,
    const ShaderProperties &additional_versions,
    const FilePath &output_file_path,
    CompiledShaderBatch &batch
)
{
    // Check that each version specified is present in the CompiledShaderBatch.
    // OR any src files have been changed since the object file was compiled.
    // if not, we need to recompile those versions.

    // TODO: Each individual item should have a timestamp internally set
    const UInt64 object_file_last_modified = output_file_path.LastModifiedTimestamp();

    UInt64 max_source_file_last_modified = 0;

    for (auto &source_file : bundle.sources) {
        max_source_file_last_modified = MathUtil::Max(max_source_file_last_modified, FilePath(source_file.second.path).LastModifiedTimestamp());
    }

    if (max_source_file_last_modified >= object_file_last_modified) {
        DebugLog(
            LogType::Info,
            "Source file in batch %s has been modified since the batch was last compiled, recompiling...\n",
            bundle.name.LookupString().Data()
        );

        batch = CompiledShaderBatch { };

        return CompileBundle(bundle, additional_versions, batch);
    }

    Array<ShaderProperties> missing_variants;
    Array<ShaderProperties> found_variants;
    std::mutex              mtx;
    Bool                    requested_found = false;

    {
        // grab each defined property, and iterate over each combination
        // now check that each combination is already in the bundle
        ForEachPermutation(bundle.versions, [&](const ShaderProperties &properties) {
            // get hashcode of this set of properties
            const HashCode properties_hash = properties.GetHashCode();

            const auto it = batch.compiled_shaders.FindIf([properties_hash](const CompiledShader &item) {
                return item.GetDefinition().GetProperties().GetHashCode() == properties_hash;
            });

            mtx.lock();

            if (it == batch.compiled_shaders.End()) {
                missing_variants.PushBack(properties);
            } else {
                found_variants.PushBack(properties);
            }

            mtx.unlock();
        });
        
        const auto it = batch.compiled_shaders.FindIf([properties_hash = additional_versions.GetHashCode()](const CompiledShader &item) {
            return item.GetDefinition().GetProperties().GetHashCode() == properties_hash;
        });

        requested_found = it != batch.compiled_shaders.End();
    }

    if (missing_variants.Any() || !requested_found) {
        // clear the batch if properties requested are missing.
        batch = CompiledShaderBatch { };

        String missing_variants_string;
        String found_variants_string;

        {
            SizeType index = 0;

            for (const ShaderProperties &missing_shader_properties : missing_variants) {
                missing_variants_string += String::ToString(missing_shader_properties.GetHashCode().Value()) + " - " + missing_shader_properties.ToString();

                if (index != missing_variants.Size() - 1) {
                    missing_variants_string += ",\n\t";
                }

                index++;
            }

        }

        {
            SizeType index = 0;

            for (const ShaderProperties &found_shader_properties : found_variants) {
                found_variants_string += String::ToString(found_shader_properties.GetHashCode().Value()) + " - " + found_shader_properties.ToString();

                if (index != found_variants.Size() - 1) {
                    found_variants_string += ",\n\t";
                }

                index++;
            }
        }

        if (should_compile_missing_variants && CanCompileShaders()) {
            DebugLog(
                LogType::Info,
                "Compiled shader is missing properties. Attempting to compile with the missing properties.\n\tRequested with properties:\n\t%s\n\n\tMissing:\n\t%s\n",
                additional_versions.ToString().Data(),
                missing_variants_string.Data()
            );

            return CompileBundle(bundle, additional_versions, batch);
        }

        DebugLog(
            LogType::Error,
            "Failed to load the compiled shader %s; Variants are missing.\n\tRequested with properties:\n\t%llu - %s\n\n\tFound:\n\t%s\n\nMissing:\n\t%s\n",
            bundle.name.LookupString().Data(),
            additional_versions.GetHashCode().Value(),
            additional_versions.ToString().Data(),
            found_variants_string.Data(),
            missing_variants_string.Data()
        );

        HYP_BREAKPOINT;

        return false;
    }

    return true;
}

Bool ShaderCompiler::LoadOrCreateCompiledShaderBatch(
    Name name,
    const ShaderProperties &properties,
    CompiledShaderBatch &out
)
{
    if (!CanCompileShaders()) {
        DebugLog(
            LogType::Warn,
            "Not compiled with GLSL compiler support... Shaders may become out of date.\n"
            "If any .hypshader files are missing, you may need to recompile the engine with glslang linked, "
            "so that they can be generated.\n"
        );
    }

    if (!m_definitions || !m_definitions->IsValid()) {
        // load for first time if no definitions loaded
        if (!LoadShaderDefinitions()) {
            return false;
        }
    }

    const String name_string(name.LookupString());

    if (!m_definitions->HasSection(name_string)) {
        // not in definitions file
        DebugLog(
            LogType::Error,
            "Section %s not found in shader definitions file\n",
            name.LookupString().Data()
        );

        return false;
    }

    Bundle bundle { name };

    // get default, platform specific shader properties
    GetPlatformSpecificProperties(bundle.versions);

    // apply each permutable property from the definitions file
    const DefinitionsFile::Section &section = m_definitions->GetSection(name_string);
    ParseDefinitionSection(section, bundle);
    
    const FilePath output_file_path = g_asset_manager->GetBasePath() / "data/compiled_shaders" / name_string + ".hypshader";

    // read file if it already exists.
    fbom::FBOMReader reader(fbom::FBOMConfig { });
    fbom::FBOMDeserializedObject deserialized;

    DebugLog(
        LogType::Info,
        "Attempting to load compiled shader %s...\n",
        output_file_path.Data()
    );

    if (auto err = reader.LoadFromFile(output_file_path, deserialized)) {
        if (CanCompileShaders()) {
            DebugLog(
                LogType::Info,
                "Could not load compiled shader at path: %s\n\tMessage: %s\n\tAttempting to compile...\n",
                output_file_path.Data(),
                err.message
            );
        } else {
            DebugLog(
                LogType::Error,
                "Failed to load compiled shader file: %s\n\tMessage: %s\n",
                output_file_path.Data(),
                err.message
            );

            return false;
        }

        if (!CompileBundle(bundle, properties, out)) {
            return false;
        }
    } else if (auto compiled_shader_batch = deserialized.Get<CompiledShaderBatch>()) {
        out = *compiled_shader_batch;
    } else {
        return false;
    }
 
    return HandleCompiledShaderBatch(bundle, properties, output_file_path, out);
}

Bool ShaderCompiler::LoadShaderDefinitions(Bool precompile_shaders)
{
    if (m_definitions && m_definitions->IsValid()) {
        return true;
    }
    
    const FilePath data_path = g_asset_manager->GetBasePath() / "data/compiled_shaders";

    if (!data_path.Exists()) {
        if (FileSystem::MkDir(data_path.Data()) != 0) {
            DebugLog(
                LogType::Error,
                "Failed to create data path at %s\n",
                data_path.Data()
            );

            return false;
        }
    }

    if (m_definitions) {
        delete m_definitions;
    }

    m_definitions = new DefinitionsFile(g_asset_manager->GetBasePath() / "shaders.def");

    if (!m_definitions->IsValid()) {
        DebugLog(
            LogType::Warn,
            "Failed to load shader definitions file at path: %s\n",
            m_definitions->GetFilePath().Data()
        );

        delete m_definitions;
        m_definitions = nullptr;

        return false;
    }

    if (!precompile_shaders) {
        return true;
    }

    Array<Bundle> bundles;

    // create a bundle for each section.
    for (const auto &it : m_definitions->GetSections()) {
        const String &key = it.key;
        const DefinitionsFile::Section &section = it.value;

        const Name name_from_string = CreateNameFromDynamicString(ANSIString(key));

        Bundle bundle { name_from_string };

        ParseDefinitionSection(section, bundle);

        bundles.PushBack(std::move(bundle));
    }

    const Bool supports_rt_shaders = g_engine->GetConfig().Get(CONFIG_RT_SUPPORTED);

    FlatMap<Bundle *, Bool> results;

    // // Compile all shaders ahead of time
    for (auto &bundle : bundles) {
        if (bundle.HasRTShaders() && !supports_rt_shaders) {
            DebugLog(
                LogType::Warn,
                "Not compiling shader bundle %s because it contains raytracing shaders and raytracing is not supported on this device.\n",
                bundle.name.LookupString().Data()
            );

            continue;
        }

        if (bundle.HasVertexShader()) {
            bundle.versions.Merge(ShaderProperties(renderer::static_mesh_vertex_attributes));
            bundle.versions.Merge(ShaderProperties(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes));
        }

        ForEachPermutation(bundle.versions.ToArray(), [&](const ShaderProperties &properties) {
            CompiledShader compiled_shader;

            results[&bundle] = GetCompiledShader(bundle.name, properties, compiled_shader);
        });
    }

    return results.Every([](const KeyValuePair<Bundle *, Bool> &it) {
        if (!it.second) {
            String permutation_string;

            DebugLog(
                LogType::Error,
                "%s: Loading of compiled shader failed with version hash %llu\n",
                it.first->name.LookupString().Data(),
                it.first->versions.GetHashCode().Value()
            );
        }

        return it.second;
    });
}

struct LoadedSourceFile
{
    ShaderModuleType          type;
    ShaderLanguage              language;
    ShaderCompiler::SourceFile  file;
    UInt64                      last_modified_timestamp;
    String                      source;

    FilePath GetOutputFilepath(const FilePath &base_path, const ShaderProperties &properties) const
    {
        return base_path / "data/compiled_shaders/tmp" / (FilePath(file.path).Basename()
            + "_" + String::ToString(properties.GetHashCode().Value()) + ".spirv");
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

Bool ShaderCompiler::CanCompileShaders() const
{
    if (!g_engine->GetConfig().Get(CONFIG_SHADER_COMPILATION)) {
        return false;
    }

#ifdef HYP_GLSLANG
    return true;
#else
    return false;
#endif
}

ShaderCompiler::ProcessResult ShaderCompiler::ProcessShaderSource(const String &source)
{
    ProcessResult result;

    Array<String> lines = source.Split('\n');

    struct ParseCustomStatementResult
    {
        Array<String> args;
        String remaining;
    };

    auto ParseCustomStatement = [](const String &start, const String &line) -> ParseCustomStatementResult
    {
        const String substr = line.Substr(start.Length());

        String args_string;

        Int parentheses_depth = 0;
        SizeType index;

        // Note: using 'Size' and 'Data' to index -- not using utf-8 chars here.
        for (index = 0; index < substr.Size(); index++) {
            if (substr.Data()[index] == ')') {
                parentheses_depth--;
            }

            if (parentheses_depth > 0) {
                args_string.Append(substr.Data()[index]);
            }

            if (substr.Data()[index] == '(') {
                parentheses_depth++;
            }

            if (parentheses_depth <= 0) {
                break;
            }
        }

        Array<String> args = args_string.Split(',');

        for (String &arg : args) {
            arg = arg.Trimmed();
        }

        return {
            std::move(args),
            substr.Substr(index + 1)
        };
    };

    for (auto &line : lines) {
        line = line.Trimmed();
    
        if (line.StartsWith("HYP_ATTRIBUTE")) {
            Array<String> parts = line.Split(' ');

            Bool optional = false;

            if (parts.Size() != 3) {
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

                while (index != parts.Front().Size() && (std::isalpha(ch = parts.Front()[index]) || ch == '_')) {
                    attribute_keyword.Append(ch);
                    ++index;
                }

                if (attribute_keyword == "HYP_ATTRIBUTE_OPTIONAL") {
                    optional = true; 
                } else if (attribute_keyword == "HYP_ATTRIBUTE") {
                    optional = false;
                } else {
                    result.errors.PushBack(ProcessError { String("Invalid attribute, unknown attribute keyword `") + attribute_keyword + "`" });

                    break;
                }

                if (index != parts.Front().Size() && ((ch = parts.Front()[index]) == '(')) {
                    ++index;
                } else {
                    result.errors.PushBack(ProcessError { "Invalid attribute, missing opening parenthesis" });

                    break;
                }

                // read integer string
                while (index != parts.Front().Size() && std::isdigit(ch = parts.Front()[index])) {
                    attribute_location.Append(ch);
                    ++index;
                }

                // if there is a comma, read the conditional define that we will use
                if (index != parts.Front().Size() && ((ch = parts.Front()[index]) == ',')) {
                    ++index;

                    String condition;
                    while (index != parts.Front().Size() && (std::isalpha(ch = parts.Front()[index]) || ch == '_')) {
                        condition.Append(ch);
                        ++index;
                    }

                    attribute_condition = condition;
                }

                if (index != parts.Front().Size() && ((ch = parts.Front()[index]) == ')')) {
                    ++index;
                } else {
                    result.errors.PushBack(ProcessError { "Invalid attribute, missing closing parenthesis" });

                    break;
                }

                if (attribute_location.Empty()) {
                    result.errors.PushBack(ProcessError { "Invalid attribute location" });

                    break;
                }
            }

            for (SizeType index = 0; index < parts[1].Size() && (std::isalnum(ch = parts[1][index]) || ch == '_'); index++) {
                attribute_type.Append(ch);
            }

            for (SizeType index = 0; index < parts[2].Size() && (std::isalnum(ch = parts[2][index]) || ch == '_'); index++) {
                attribute_name.Append(ch);
            }

            VertexAttributeDefinition attribute_definition;
            attribute_definition.name = attribute_name;
            attribute_definition.type_class = attribute_type;
            attribute_definition.location = std::atoi(attribute_location.Data());

            if (optional) {
                result.optional_attributes.PushBack(attribute_definition);

                if (attribute_condition.HasValue()) {
                    result.processed_source += "#if defined(" + attribute_condition.Get() + ") && " + attribute_condition.Get() + "\n";

                    attribute_definition.condition = attribute_condition;
                } else {
                    result.processed_source += "#ifdef HYP_ATTRIBUTE_" + attribute_definition.name + "\n";
                }
            } else {
                result.required_attributes.PushBack(attribute_definition);
            }

            result.processed_source += "layout(location=" + String::ToString(attribute_definition.location) + ") in " + attribute_definition.type_class + " " + attribute_definition.name + ";\n";
            
            if (optional) {
                result.processed_source += "#endif\n";
            }
        } else if (line.StartsWith("HYP_DESCRIPTOR")) {
            String command_str;

            for (SizeType index = 0; index < line.Size(); index++) {
                if (std::isalnum(line.Data()[index]) || line.Data()[index] == '_') {
                    command_str.Append(line.Data()[index]);
                } else {
                    break;
                }
            }

            renderer::DescriptorSlot slot = renderer::DESCRIPTOR_SLOT_NONE;
            DescriptorUsageFlags flags = DESCRIPTOR_USAGE_FLAG_NONE;

            if (command_str == "HYP_DESCRIPTOR_SRV") {
                slot = renderer::DESCRIPTOR_SLOT_SRV;
            } else if (command_str == "HYP_DESCRIPTOR_UAV") {
                slot = renderer::DESCRIPTOR_SLOT_UAV;
            } else if (command_str == "HYP_DESCRIPTOR_CBUFF" || command_str == "HYP_DESCRIPTOR_CBUFF_DYNAMIC") {
                slot = renderer::DESCRIPTOR_SLOT_CBUFF;

                if (command_str == "HYP_DESCRIPTOR_CBUFF_DYNAMIC") {
                    flags |= DESCRIPTOR_USAGE_FLAG_DYNAMIC;
                }
            } else if (command_str == "HYP_DESCRIPTOR_SSBO" || command_str == "HYP_DESCRIPTOR_SSBO_DYNAMIC") {
                slot = renderer::DESCRIPTOR_SLOT_SSBO;

                if (command_str == "HYP_DESCRIPTOR_SSBO_DYNAMIC") {
                    flags |= DESCRIPTOR_USAGE_FLAG_DYNAMIC;
                }
            } else if (command_str == "HYP_DESCRIPTOR_ACCELERATION_STRUCTURE") {
                slot = renderer::DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE;
            } else if (command_str == "HYP_DESCRIPTOR_SAMPLER") {
                slot = renderer::DESCRIPTOR_SLOT_SAMPLER;
            } else {
                result.errors.PushBack(ProcessError { "Invalid descriptor slot. Must match HYP_DESCRIPTOR_<Type> " });

                break;
            }

            Array<String> parts = line.Split(' ');

            String descriptor_name,
                set_name,
                slot_str;

            HashMap<String, String> params;

            auto parse_result = ParseCustomStatement(command_str, line);

            if (parse_result.args.Size() < 2) {
                result.errors.PushBack(ProcessError { "Invalid descriptor: Requires format HYP_DESCRIPTOR_<TYPE>(set, name)" });

                break;
            }

            set_name = parse_result.args[0];
            descriptor_name = parse_result.args[1];

            if (parse_result.args.Size() > 2) {
                for (SizeType index = 2; index < parse_result.args.Size(); index++) {
                    Array<String> split = parse_result.args[index].Split('=');

                    for (String &part : split) {
                        part = part.Trimmed();
                    }

                    if (split.Size() != 2) {
                        result.errors.PushBack(ProcessError { "Invalid parameter: Requires format <key>=<value>" });

                        break;
                    }

                    const String &key = split[0];
                    const String &value = split[1];

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

            // DebugLog(LogType::Debug, "Create Descriptor usage: %s %s %u %d\n", set_name.Data(), descriptor_name.Data(), slot, is_dynamic);

            // Bool is_custom_descriptor_set = false;

            // auto descriptor_set_declaration = renderer::g_static_descriptor_table->FindDescriptorSetDeclaration(usage.set_name);

            // if (descriptor_set_declaration) {
            //     auto descriptor_declaration = descriptor_set_declaration->FindDescriptorDeclaration(usage.descriptor_name);

            //     if (descriptor_declaration) {
            //         result.final_source += "layout(set=HYP_DESCRIPTOR_SET_INDEX_" + set_name + ", binding=HYP_DESCRIPTOR_INDEX_" + set_name + "_" + descriptor_name + ") " + parse_result.remaining + "\n";
            //     } else {
            //         result.errors.PushBack(ProcessError { String("Invalid descriptor ") + usage.descriptor_name.LookupString().Data() });

            //         break;
            //     }
            // } else {
            //     is_custom_descriptor_set = true;
            // }

            Array<String> additional_params;

            if (usage.params.Contains("format")) {
                additional_params.PushBack(usage.params.At("format"));
            }

            result.processed_source += "layout(set=HYP_DESCRIPTOR_SET_INDEX_" + set_name + ", binding=HYP_DESCRIPTOR_INDEX_" + set_name + "_" + descriptor_name + (additional_params.Any() ? (", " + String::Join(additional_params, ", ")) : "") + ") " + parse_result.remaining + "\n";

            result.descriptor_usages.PushBack(usage);
        } else {
            result.processed_source += line + '\n';
        }
    }

    return result;
}

Bool ShaderCompiler::CompileBundle(
    Bundle &bundle,
    const ShaderProperties &additional_versions,
    CompiledShaderBatch &out
)
{
    if (!CanCompileShaders()) {
        return false;
    }

    // run with spirv-cross
    FileSystem::MkDir((g_asset_manager->GetBasePath() / "data/compiled_shaders/tmp").Data());

    Array<LoadedSourceFile> loaded_source_files;
    loaded_source_files.Resize(bundle.sources.Size());

    Array<Array<VertexAttributeDefinition>> required_vertex_attributes;
    required_vertex_attributes.Resize(bundle.sources.Size());

    Array<Array<VertexAttributeDefinition>> optional_vertex_attributes;
    optional_vertex_attributes.Resize(bundle.sources.Size());

    Array<Array<DescriptorUsage>> descriptor_usages;
    descriptor_usages.Resize(bundle.sources.Size());

    Array<Array<ProcessError>> process_errors;
    process_errors.Resize(bundle.sources.Size());

    TaskBatch task_batch;

    for (SizeType index = 0; index < bundle.sources.Size(); index++) {
        task_batch.AddTask([this, index, &bundle, &required_vertex_attributes, &optional_vertex_attributes, &loaded_source_files, &process_errors, &descriptor_usages](...) {
            const auto &it = bundle.sources.AtIndex(index);

            const FilePath filepath = it.second.path;

            Reader reader;

            if (!filepath.Open(reader)) {
                // could not open file!
                DebugLog(
                    LogType::Error,
                    "Failed to open shader source file at %s\n",
                    filepath.Data()
                );

                process_errors[index] = {
                    ProcessError { "Failed to open source file"}
                };

                return;
            }

            ByteBuffer byte_buffer = reader.ReadBytes();

            String source_string = String(byte_buffer);
            ProcessResult result = ProcessShaderSource(source_string);

            if (result.errors.Any()) {
                DebugLog(
                    LogType::Error,
                    "%u shader processing errors:\n",
                    UInt(result.errors.Size())
                );

                process_errors[index] = result.errors;

                return;
            }

            required_vertex_attributes[index] = result.required_attributes;
            optional_vertex_attributes[index] = result.optional_attributes;
            descriptor_usages[index] = std::move(result.descriptor_usages);

            String final_source = result.processed_source;

            loaded_source_files[index] = LoadedSourceFile {
                .type                       = it.first,
                .language                   = filepath.EndsWith("hlsl") ? ShaderLanguage::HLSL : ShaderLanguage::GLSL,
                .file                       = it.second,
                .last_modified_timestamp    = filepath.LastModifiedTimestamp(),
                .source                     = final_source
            };
        });
    }

    TaskSystem::GetInstance().EnqueueBatch(&task_batch);
    task_batch.AwaitCompletion();

    Array<ProcessError> all_process_errors;

    for (const auto &error_list : process_errors) {
        all_process_errors.Concat(error_list);
    }

    if (!all_process_errors.Empty()) {
        for (const auto &error : all_process_errors) {
            DebugLog(
                LogType::Error,
                "\t%s\n",
                error.error_message.Data()
            );
        }

        return false;
    }

    // Merge all descriptor usages together
    for (const Array<DescriptorUsage> &descriptor_usage_list : descriptor_usages) {
        bundle.descriptor_usages.Merge(descriptor_usage_list);
    }

    // grab each defined property, and iterate over each combination
    ShaderProperties final_versions;
    final_versions.Merge(bundle.versions);

    // for (const auto &required_attribute : required_vertex_attributes) {
    //     String attribute_prop_name = String("HYP_ATTRIBUTE_") + required_attribute.name;

    //     final_versions.Set(ShaderProperty(attribute_prop_name, false, SHADER_PROPERTY_FLAG_VERTEX_ATTRIBUTE));
    // }

    // for (const VertexAttributeDefinition &optional_attribute : optional_vertex_attributes) {
    //     String attribute_prop_name = String("HYP_ATTRIBUTE_") + optional_attribute.name;

    //     final_versions.Set(ShaderProperty(attribute_prop_name, true));
    // }

    { // Lookup vertex attribute names

        VertexAttributeSet required_vertex_attribute_set;
        VertexAttributeSet optional_vertex_attribute_set;

        for (const auto &definitions : required_vertex_attributes) {
            for (const VertexAttributeDefinition &definition : definitions) {
                VertexAttribute::Type type;

                if (!FindVertexAttributeForDefinition(definition.name, type)) {
                    DebugLog(
                        LogType::Error,
                        "Invalid vertex attribute definition, %s\n",
                        definition.name.Data()
                    );

                    continue;
                }

                required_vertex_attribute_set |= type;
            }
        }
        
        for (const auto &definitions : optional_vertex_attributes) {
            for (const VertexAttributeDefinition &definition : definitions) {
                VertexAttribute::Type type;

                if (!FindVertexAttributeForDefinition(definition.name, type)) {
                    DebugLog(
                        LogType::Error,
                        "Invalid vertex attribute definition, %s\n",
                        definition.name.Data()
                    );

                    continue;
                }

                optional_vertex_attribute_set |= type;
            }
        }

        final_versions.SetRequiredVertexAttributes(required_vertex_attribute_set);
        final_versions.SetOptionalVertexAttributes(optional_vertex_attribute_set);
    }

    // copy passed properties
    final_versions.Merge(additional_versions);

    DebugLog(
        LogType::Info,
        "Compiling shader bundle for shader %s\n",
        bundle.name.LookupString().Data()
    );

    // update versions to include vertex attribute properties
    bundle.versions = final_versions;

    std::mutex fs_mutex;
    std::mutex compiled_shaders_mutex;
    std::mutex error_messages_mutex;

    AtomicVar<UInt> num_compiled_permutations { 0u };

    String descriptor_table_defines;

    renderer::DescriptorTable descriptor_table = bundle.descriptor_usages.BuildDescriptorTable();

    for (renderer::DescriptorSetDeclaration &descriptor_set_declaration : descriptor_table.GetDescriptorSetDeclarations()) {
        descriptor_table_defines += "#define HYP_DESCRIPTOR_SET_INDEX_" + String(descriptor_set_declaration.name.LookupString().Data()) + " " + String::ToString(descriptor_set_declaration.set_index) + "\n";

        for (const Array<renderer::DescriptorDeclaration> &descriptor_declarations : descriptor_set_declaration.slots) {
            for (const renderer::DescriptorDeclaration &descriptor_declaration : descriptor_declarations) {
                DebugLog(LogType::Debug, "Declaration : %s\n", descriptor_declaration.name.LookupString().Data());
                const UInt flat_index = descriptor_set_declaration.CalculateFlatIndex(descriptor_declaration.slot, descriptor_declaration.name);
                AssertThrow(flat_index != UInt(-1));

                descriptor_table_defines += "\t#define HYP_DESCRIPTOR_INDEX_" + String(descriptor_set_declaration.name.LookupString().Data()) + "_" + descriptor_declaration.name.LookupString().Data() + " " + String::ToString(flat_index) + "\n";
            }
        }
    }

    DebugLog(LogType::Debug, "Descriptor table defines = %s\n", descriptor_table_defines.Data());

    // compile shader with each permutation of properties
    ForEachPermutation(final_versions, [&](const ShaderProperties &properties) {
        CompiledShader compiled_shader(
            ShaderDefinition {
                bundle.name,
                properties,
                bundle.descriptor_usages
            },
            bundle.entry_point_name
        );

        Bool any_files_compiled = false;

        for (const LoadedSourceFile &item : loaded_source_files) {
            // check if a file exists w/ same hash
            
            const auto output_filepath = item.GetOutputFilepath(
                g_asset_manager->GetBasePath(),
                properties
            );

            if (output_filepath.Exists()) {
                // file exists and is older than the original source file; no need to build
                if (output_filepath.LastModifiedTimestamp() >= item.last_modified_timestamp) {
                    Reader reader;

                    if (output_filepath.Open(reader)) {
                        DebugLog(
                            LogType::Info,
                            "Shader source (%s) has not been modified since binary was generated. Reusing shader binary at path: %s\n\tProperties: [%s]\n",
                            item.file.path.Data(),
                            output_filepath.Data(),
                            properties.ToString().Data()
                        );
                        
                        compiled_shader.modules[item.type] = reader.ReadBytes();

                        continue;
                    }

                    DebugLog(
                        LogType::Warn,
                        "File %s seems valid for reuse but could not be opened. Attempting to rebuild...\n\tProperties: [%s]\n",
                        output_filepath.Data(),
                        properties.ToString().Data()
                    );
                }
            }

            String variable_properties_string;
            String static_properties_string;

            for (const ShaderProperty &property : properties.ToArray()) {
                if (property.is_permutation) {
                    if (!variable_properties_string.Empty()) {
                        variable_properties_string += ", ";
                    }

                    variable_properties_string += property.name;
                } else {
                    if (!static_properties_string.Empty()) {
                        static_properties_string += ", ";
                    }

                    static_properties_string += property.name;
                }
            }

            DebugLog(
                LogType::Info,
                "Compiling shader %s\n\tVariable properties: [%s]\n\tStatic properties: [%s]\n\tProperties hash: %llu\n",
                output_filepath.Data(),
                variable_properties_string.Data(),
                static_properties_string.Data(),
                properties.GetHashCode().Value()
            );

            ByteBuffer byte_buffer;

            Array<String> error_messages;

            // set directory to the directory of the shader
            const FilePath dir = g_asset_manager->GetBasePath() / FilePath::Relative(FilePath(item.file.path).BasePath(), g_asset_manager->GetBasePath());

            String preamble;
            
            { // Build preamble string.
                // Start off with the descriptor table defines
                // These will be used to generate the descriptor table layout
                preamble += descriptor_table_defines + "\n\n";

                for (const VertexAttribute &attribute : properties.GetRequiredVertexAttributes().BuildAttributes()) {
                    preamble += String("#define HYP_ATTRIBUTE_") + attribute.name + "\n";
                }

                // We do not do the same for Optional attributes, as they have not been instantiated at this point in time.
                // before compiling the shader, they should have all been made Required.
                
                for (const ShaderProperty &property : properties.GetPropertySet()) {
                    if (property.name.Empty()) {
                        continue;
                    }

                    preamble += "#define " + property.name + "\n";
                }
            }

            fs_mutex.lock();
            FileSystem::PushDirectory(dir);
            byte_buffer = CompileToSPIRV(item.type, item.language, std::move(preamble), item.source, item.file.path, properties, error_messages);
            FileSystem::PopDirectory();
            fs_mutex.unlock();

            if (byte_buffer.Empty()) {
                DebugLog(
                    LogType::Error,
                    "Failed to compile file %s with version hash %u!\n!",
                    item.file.path.Data(),
                    properties.GetHashCode().Value()
                );

                error_messages_mutex.lock();
                out.error_messages.Concat(std::move(error_messages));
                error_messages_mutex.unlock();

                return;
            }

            // write the spirv to the output file
            FileByteWriter spirv_writer(output_filepath.Data());

            if (!spirv_writer.IsOpen()) {
                DebugLog(
                    LogType::Error,
                    "Could not open file %s for writing!\n",
                    output_filepath.Data()
                );

                return;
            }

            spirv_writer.Write(byte_buffer.Data(), byte_buffer.Size());
            spirv_writer.Close();

            any_files_compiled = true;

            compiled_shader.modules[item.type] = byte_buffer;
        }

        num_compiled_permutations.Increment(UInt(any_files_compiled), MemoryOrder::RELAXED);

        compiled_shaders_mutex.lock();
        out.compiled_shaders.PushBack(std::move(compiled_shader));
        compiled_shaders_mutex.unlock();
    });

    const FilePath final_output_path = g_asset_manager->GetBasePath() / "data/compiled_shaders" / String(bundle.name.LookupString()) + ".hypshader";

    FileByteWriter byte_writer(final_output_path.Data());

    fbom::FBOMWriter writer;
    writer.Append(out);

    auto err = writer.Emit(&byte_writer);
    byte_writer.Close();

    if (err.value != fbom::FBOMResult::FBOM_OK) {
        return false;
    }

    m_cache.Set(bundle.name, out);

    if (num_compiled_permutations.Get(MemoryOrder::RELAXED) != 0) {
        DebugLog(
            LogType::Info,
            "Compiled %u new variants for shader %s to: %s\n",
            num_compiled_permutations.Get(MemoryOrder::RELAXED),
            bundle.name.LookupString().Data(),
            final_output_path.Data()
        );
    }

    return true;
}

CompiledShader ShaderCompiler::GetCompiledShader(Name name)
{
    ShaderProperties properties { };

    return GetCompiledShader(name, properties);
}

CompiledShader ShaderCompiler::GetCompiledShader(Name name, const ShaderProperties &properties)
{
    CompiledShader compiled_shader;

    GetCompiledShader(
        name,
        properties,
        compiled_shader
    );

    return compiled_shader;
}

Bool ShaderCompiler::GetCompiledShader(
    Name name,
    const ShaderProperties &properties,
    CompiledShader &out
)
{
    ShaderProperties final_properties;
    GetPlatformSpecificProperties(final_properties);
    final_properties.Merge(properties);

    const HashCode final_properties_hash = final_properties.GetHashCode();
    
    if (m_cache.GetShaderInstance(name, final_properties_hash.Value(), out)) {
        return true;
    }

    CompiledShaderBatch batch;
    
    if (!LoadOrCreateCompiledShaderBatch(name, final_properties, batch)) {
        DebugLog(
            LogType::Error,
            "Failed to attempt loading of shader batch: %s\n\tRequested instance with properties: [%s]\n",
            name.LookupString().Data(),
            final_properties.ToString().Data()
        );

        return false;
    }

    // set in cache so we can use it later
    m_cache.Set(name, batch);

    // make sure we properly created it

    auto it = batch.compiled_shaders.FindIf([final_properties_hash](const CompiledShader &compiled_shader)
    {
        return compiled_shader.GetDefinition().GetProperties().GetHashCode() == final_properties_hash;
    });

    if (it == batch.compiled_shaders.End()) {
        DebugLog(
            LogType::Error,
            "Hash calculation for shader %s does not match %llu! Invalid shader property combination.\n\tRequested instance with properties: [%s]\n",
            name.LookupString().Data(),
            final_properties_hash.Value(),
            final_properties.ToString().Data()
        );

        return false;
    }

    out = *it;

    DebugLog(
        LogType::Debug,
        "Selected shader %s for hash %llu.\n\tRequested instance with properties: [%s]\n",
        name.LookupString().Data(),
        final_properties_hash.Value(),
        final_properties.ToString().Data()
    );

    return true;
}

} // namespace hyperion::v2
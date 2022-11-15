#include <util/shader_compiler/ShaderCompiler.hpp>
#include <asset/ByteWriter.hpp>
#include <asset/serialization/fbom/FBOM.hpp>
#include <asset/serialization/fbom/marshals/ShaderBundleMarshal.hpp>
#include <math/MathUtil.hpp>
#include <util/definitions/DefinitionsFile.hpp>
#include <util/fs/FsUtil.hpp>
#include <util/Defines.hpp>

#ifdef HYP_GLSLANG

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#endif
namespace hyperion::v2 {

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
    ShaderModule::Type type,
    ShaderLanguage language,
    const char *source,
    const char *filename,
    const Array<String> &version_strings,
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

    switch (type) {
    case ShaderModule::Type::VERTEX:
        stage = GLSLANG_STAGE_VERTEX;
        break;
    case ShaderModule::Type::FRAGMENT:
        stage = GLSLANG_STAGE_FRAGMENT;
        break;
    case ShaderModule::Type::GEOMETRY:
        stage = GLSLANG_STAGE_GEOMETRY;
        break;
    case ShaderModule::Type::COMPUTE:
        stage = GLSLANG_STAGE_COMPUTE;
        break;
    case ShaderModule::Type::TASK:
        stage = GLSLANG_STAGE_TASK_NV;
        break;
    case ShaderModule::Type::MESH:
        stage = GLSLANG_STAGE_MESH_NV;
        break;
    case ShaderModule::Type::TESS_CONTROL:
        stage = GLSLANG_STAGE_TESSCONTROL;
        break;
    case ShaderModule::Type::TESS_EVAL:
        stage = GLSLANG_STAGE_TESSEVALUATION;
        break;
    case ShaderModule::Type::RAY_GEN:
        stage = GLSLANG_STAGE_RAYGEN_NV;
        break;
    case ShaderModule::Type::RAY_INTERSECT:
        stage = GLSLANG_STAGE_INTERSECT_NV;
        break;
    case ShaderModule::Type::RAY_ANY_HIT:
        stage = GLSLANG_STAGE_ANYHIT_NV;
        break;
    case ShaderModule::Type::RAY_CLOSEST_HIT:
        stage = GLSLANG_STAGE_CLOSESTHIT_NV;
        break;
    case ShaderModule::Type::RAY_MISS:
        stage = GLSLANG_STAGE_MISS_NV;
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
        .code = source,
        .default_version = int(spirv_version),
        .default_profile = GLSLANG_CORE_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = reinterpret_cast<const glslang_resource_t *>(&default_resources),
    };

    glslang_shader_t *shader = glslang_shader_create(&input);

    String version_strings_combined;
    
    for (const auto &str : version_strings) {
        if (str.Empty()) {
            continue;
        }

        version_strings_combined += "#define " + str + "\n";
    }

    glslang_shader_set_preamble(shader, version_strings_combined.Data());

    if (!glslang_shader_preprocess(shader, &input))	{
        GLSL_ERROR(LogType::Error, "GLSL preprocessing failed %s\n", filename);
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_info_log(shader));
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_info_debug_log(shader));
        GLSL_ERROR(LogType::Error, "%s\n", input.code);
        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    if (!glslang_shader_parse(shader, &input)) {
        GLSL_ERROR(LogType::Error, "GLSL parsing failed %s\n", filename);
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_info_log(shader));
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_info_debug_log(shader));
        GLSL_ERROR(LogType::Error, "%s\n", glslang_shader_get_preprocessed_code(shader));
        glslang_shader_delete(shader);

        return ByteBuffer();
    }

    glslang_program_t *program = glslang_program_create();
    glslang_program_add_shader(program, shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        GLSL_ERROR(LogType::Error, "GLSL linking failed %s %s\n", filename, source);
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
        GLSL_ERROR(LogType::Error, "(%s) %s\b", filename, spirv_messages);
    }

    glslang_program_delete(program);
    glslang_shader_delete(shader);

#undef GLSL_ERROR

    return shader_module;
}

#else


static ByteBuffer CompileToSPIRV(
    ShaderModule::Type type,
    ShaderLanguage language,
    const char *source, const char *filename, 
    const Array<String> &version_strings,
    Array<String> &error_messages
)
{
    return ByteBuffer();
}

#endif

static const FlatMap<String, ShaderModule::Type> shader_type_names = {
    { "vert", ShaderModule::Type::VERTEX },
    { "frag", ShaderModule::Type::FRAGMENT },
    { "geom", ShaderModule::Type::GEOMETRY },
    { "comp", ShaderModule::Type::COMPUTE },
    { "rgen", ShaderModule::Type::RAY_GEN },
    { "rchit", ShaderModule::Type::RAY_CLOSEST_HIT },
    { "rahit", ShaderModule::Type::RAY_ANY_HIT },
    { "rmiss", ShaderModule::Type::RAY_MISS },
    { "rint", ShaderModule::Type::RAY_INTERSECT },
    { "tesc", ShaderModule::Type::TESS_CONTROL },
    { "mesh", ShaderModule::Type::MESH },
    { "task", ShaderModule::Type::TASK }
};

static void ForEachPermutation(
    const Array<String> &versions,
    Proc<void, const Array<String> &> callback
)
{
    SizeType num_permutations = 1ull << versions.Size();

    for (SizeType i = 0; i < num_permutations; i++) {
        Array<String> tmp_versions;
        tmp_versions.Reserve(MathUtil::BitCount(i));

        for (SizeType j = 0; j < versions.Size(); j++) {
            if (i & (1ull << j)) {
                tmp_versions.PushBack(versions[j]);
            }
        }

        callback(tmp_versions);
    }
}

ShaderCompiler::ShaderCompiler(Engine *engine)
    : m_engine(engine),
      m_definitions(nullptr)
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

void ShaderCompiler::GetDefaultVersions(ShaderProps &versions) const
{
#if defined(HYP_VULKAN) && HYP_VULKAN
    versions.Set("HYP_VULKAN");

    constexpr UInt vulkan_version = HYP_VULKAN_API_VERSION;
    
    switch (vulkan_version) {
    case VK_API_VERSION_1_1:
        versions.Set("HYP_VULKAN_1_1");
        break;
    case VK_API_VERSION_1_2:
        versions.Set("HYP_VULKAN_1_2");
        break;
#ifdef VK_API_VERSION_1_3
    case VK_API_VERSION_1_3:
        versions.Set("HYP_VULKAN_1_3");
        break;
#endif
    default:
        break;
    }
#elif defined(HYP_DX12) && HYP_DX12
    versions.Set("DX12");
#endif
}

void ShaderCompiler::ParseDefinitionSection(
    const DefinitionsFile::Section &section,
    ShaderCompiler::Bundle &bundle
)
{
    for (const auto &section_it : section) {
        if (section_it.first == "permute") {
            // set each property
            for (const auto &element : section_it.second.elements) {
                bundle.versions.Set(element, true);
            }
        } else if (shader_type_names.Contains(section_it.first)) {
            bundle.sources[shader_type_names.At(section_it.first)] = SourceFile {
                m_engine->GetAssetManager().GetBasePath() / "vkshaders" / section_it.second.GetValue()
            };
        } else {
            DebugLog(
                LogType::Warn,
                "Unknown property in shader definition file: %s\n",
                section_it.first.Data()
            );
        }
    }
}

bool ShaderCompiler::LoadOrCreateCompiledShaderBatch(
    const String &name,
    const ShaderProps &additional_versions,
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

    if (!m_definitions->HasSection(name)) {
        // not in definitions file
        DebugLog(
            LogType::Error,
            "Section %s not found in shader definitions file\n",
            name.Data()
        );

        return false;
    }

    Bundle bundle { .name = name };
    GetDefaultVersions(bundle.versions);

    const auto &section = m_definitions->GetSection(name);

    ParseDefinitionSection(section, bundle);

    Array<String> added_versions;

    for (const auto &item : additional_versions) {
        if (!bundle.versions.Get(item)) {
            bundle.versions.Set(item, true);

            added_versions.PushBack(item);
        }
    }

    if (added_versions.Any()) {
        String added_versions_string;

        for (SizeType index = 0; index < added_versions.Size(); index++) {
            added_versions_string += added_versions[index];

            if (index != added_versions.Size() - 1) {
                added_versions_string += ", ";
            }
        }

        // dynamically add the file
        DebugLog(
            LogType::Warn,
            "Dynamic shader compilation for shader %s. Consider adding the following keys to the shader definition file:\n\t%s\n",
            name.Data(),
            added_versions_string.Data()
        );
    }

    
    const auto output_file_path = m_engine->GetAssetManager().GetBasePath() / "data/compiled_shaders" / name + ".hypshader";
    const auto version_hash = bundle.versions.GetHashCode();

    // read file if it already exists.
    fbom::FBOMReader reader(m_engine, fbom::FBOMConfig { });
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
        }

        return CompileBundle(bundle, out);
    }
 
    if (auto compiled_shader_batch = deserialized.Get<CompiledShaderBatch>()) {
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
                name.Data()
            );

            return CompileBundle(bundle, out);
        }

        // compile each..
        Array<String> versions;
        versions.Reserve(bundle.versions.Size());

        for (auto &item : bundle.versions) {
            versions.PushBack(item);
        }

        Array<String> missing_versions;

        // now check that each combination is already in the bundle
        ForEachPermutation(versions, [&](const Array<String> &items) {
            // get hashcode of this set of properties
            UInt64 version_hash = items.GetHashCode().Value();

            auto it = compiled_shader_batch->compiled_shaders.FindIf([version_hash](const CompiledShader &item) {
                return item.version_hash == version_hash;
            });

            if (it == compiled_shader_batch->compiled_shaders.End()) {
                String missing_versions_string = "[";

                for (SizeType index = 0; index < items.Size(); index++) {
                    missing_versions_string += items[index];

                    if (index == missing_versions.Size() - 1) {
                        missing_versions_string += ", ";
                    }
                }

                missing_versions_string += "]";

                missing_versions.PushBack(std::move(missing_versions_string));
            }
        });

        if (missing_versions.Any()) {
            String missing_versions_string;

            for (SizeType index = 0; index < missing_versions.Size(); index++) {
                missing_versions_string += missing_versions[index];

                if (index == missing_versions.Size() - 1) {
                    missing_versions_string += ", ";
                }
            }

            DebugLog(
                LogType::Info,
                "Compiled shader is missing versions. Attempting to compile the missing versions.\n\tVersions: [%s]\n",
                missing_versions_string.Data()
            );

            return CompileBundle(bundle, out);
        }

        out = *compiled_shader_batch;

        return true;
    }

    DebugLog(
        LogType::Error,
        "Failed to load the compiled shader %s, and it could not be recompiled.\n",
        name.Data()
    );

    return false;
}

bool ShaderCompiler::LoadShaderDefinitions()
{
    const FilePath data_path = m_engine->GetAssetManager().GetBasePath() / "data/compiled_shaders";

    if (!data_path.Exists()) {
        if (FileSystem::Mkdir(data_path.Data()) != 0) {
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

    m_definitions = new DefinitionsFile(m_engine->GetAssetManager().GetBasePath() / "shaders.def");

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

    Array<Bundle> bundles;

    // create a bundle for each section.
    for (const auto &it : m_definitions->GetSections()) {
        const auto &key = it.first;
        auto &section = it.second;

        Bundle bundle { .name = key };

        ParseDefinitionSection(section, bundle);

        bundles.PushBack(std::move(bundle));
    }

    const bool supports_rt_shaders = m_engine->GetConfig().Get(CONFIG_RT_SUPPORTED);

    FlatMap<Bundle *, bool> results;

    for (SizeType index = 0; index < bundles.Size(); index++) {
        auto &bundle = bundles[index];

        if (bundle.HasRTShaders() && !supports_rt_shaders) {
            DebugLog(
                LogType::Warn,
                "Not compiling shader bundle %s because it contains raytracing shaders and raytracing is not supported on this device.\n",
                bundle.name.Data()
            );

            continue;
        }

        CompiledShader compiled_shader;

        results[&bundle] = GetCompiledShader(bundle.name, bundle.versions, compiled_shader);
    }

    return results.Every([](const KeyValuePair<Bundle *, bool> &it) {
        if (!it.second) {
            String permutation_string;

            DebugLog(
                LogType::Error,
                "%s: Loading of compiled shader failed with version hash %llu\n",
                it.first->name.Data(),
                it.first->versions.GetHashCode().Value()
            );
        }

        return it.second;
    });

    // bundles.ParallelForEach(
    //     m_engine->task_system,
    //     [this, &results](const Bundle &bundle, UInt index, UInt) {
    //         results[index] = CompileBundle(bundle);
    //     }
    // );
}

struct LoadedSourceFile
{
    ShaderModule::Type type;
    ShaderLanguage language;
    ShaderCompiler::SourceFile file;
    UInt64 last_modified_timestamp;
    String original_source;

    FilePath GetOutputFilepath(const FilePath &base_path, HashCode version_hash) const
    {
        return base_path / "data/compiled_shaders/tmp" / (FilePath(file.path).Basename()
            + "_" + String::ToString(version_hash.Value()) + ".shc");
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(language);
        hc.Add(file);
        hc.Add(last_modified_timestamp);
        hc.Add(original_source);

        return hc;
    }
};

bool ShaderCompiler::CanCompileShaders() const
{
    if (!m_engine->GetConfig().Get(CONFIG_SHADER_COMPILATION)) {
        return false;
    }

#ifdef HYP_GLSLANG
    return true;
#else
    return false;
#endif
}

bool ShaderCompiler::CompileBundle(const Bundle &bundle, CompiledShaderBatch &out)
{
    if (!CanCompileShaders()) {
        return false;
    }

    // run with spirv-cross
    FileSystem::Mkdir((m_engine->GetAssetManager().GetBasePath() / "data/compiled_shaders/tmp").Data());

    Array<LoadedSourceFile> loaded_source_files;
    loaded_source_files.Reserve(bundle.sources.Size());

    for (auto &it : bundle.sources) {
        const FilePath filepath = it.second.path;
        auto source_stream = filepath.Open();

        if (!source_stream.IsOpen()) {
            // could not open file!
            DebugLog(
                LogType::Error,
                "Failed to open shader source file at %s\n",
                filepath.Data()
            );

            return false;
        }

        ByteBuffer byte_buffer = source_stream.ReadBytes();

        loaded_source_files.PushBack(LoadedSourceFile {
            .type = it.first,
            .language = filepath.EndsWith("hlsl") ? ShaderLanguage::HLSL : ShaderLanguage::GLSL,
            .file = it.second,
            .last_modified_timestamp = filepath.LastModifiedTimestamp(),
            .original_source = String(byte_buffer)
        });
    }

    const SizeType num_permutations = 1ull << bundle.versions.Size();
    SizeType num_compiled_permutations = 0;

    if (num_permutations >= 64) {
        // too many permutations
        DebugLog(
            LogType::Error,
            "Too many shader permutations for shader %s\n",
            bundle.name.Data()
        );

        return false;
    }

    // compile each..
    Array<String> versions;
    versions.Reserve(bundle.versions.Size());

    for (auto &item : bundle.versions) {
        versions.PushBack(item);
    }

    DebugLog(
        LogType::Info,
        "Compiling shader bundle for shader %s (%llu variants)\n",
        bundle.name.Data(),
        num_permutations
    );

    // compile shader with each permutation of properties
    ForEachPermutation(versions, [&](const Array<String> &version_strings) {
        // get hashcode of this set of properties
        HashCode version_hash = version_strings.GetHashCode();

        CompiledShader compiled_shader {
            .version_hash = version_hash.Value()
        };

        bool any_files_compiled = false;

        for (const auto &item : loaded_source_files) {
            // check if a file exists w/ same hash
            
            const auto output_filepath = item.GetOutputFilepath(
                m_engine->GetAssetManager().GetBasePath(),
                version_hash
            );

            if (output_filepath.Exists()) {
                // file exists and is older than the original source file; no need to build
                if (output_filepath.LastModifiedTimestamp() >= item.last_modified_timestamp) {
                    if (auto stream = output_filepath.Open()) {
                        DebugLog(
                            LogType::Info,
                            "Reusing shader binary at path: %s\n",
                            output_filepath.Data()
                        );

                        compiled_shader.modules[item.type] = stream.ReadBytes();

                        continue;
                    } else {
                        DebugLog(
                            LogType::Warn,
                            "File %s seems valid for reuse but could not be opened. Attempting to rebuild...\n",
                            output_filepath.Data()
                        );
                    }
                }
            }

            String permutation_string;

            for (SizeType index = 0; index < version_strings.Size(); index++) {
                permutation_string += version_strings[index];

                if (index != version_strings.Size() - 1) {
                    permutation_string += ", ";
                }
            }

            DebugLog(
                LogType::Info,
                "Compiling shader %s with permutation [%s]...\n",
                output_filepath.Data(),
                permutation_string.Data()
            );

            ByteBuffer byte_buffer;

            Array<String> error_messages;

            // set directory to the directory of the shader
            const auto dir = m_engine->GetAssetManager().GetBasePath() / FilePath::Relative(FilePath(item.file.path).BasePath(), m_engine->GetAssetManager().GetBasePath());

            FileSystem::PushDirectory(dir);
            byte_buffer = CompileToSPIRV(item.type, item.language, item.original_source.Data(), item.file.path.Data(), version_strings, error_messages);
            FileSystem::PopDirectory();

            if (byte_buffer.Empty()) {
                DebugLog(
                    LogType::Error,
                    "Failed to compile file %s with version hash %u!\n!",
                    item.file.path.Data(),
                    version_hash.Value()
                );

                out.error_messages.Concat(std::move(error_messages));

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

        num_compiled_permutations += static_cast<UInt>(any_files_compiled);
        out.compiled_shaders.PushBack(std::move(compiled_shader));
    });

    const FilePath final_output_path = m_engine->GetAssetManager().GetBasePath() / "data/compiled_shaders" / bundle.name + ".hypshader";

    FileByteWriter byte_writer(final_output_path.Data());

    fbom::FBOMWriter writer;
    writer.Append(out);

    auto err = writer.Emit(&byte_writer);
    byte_writer.Close();

    if (err.value != fbom::FBOMResult::FBOM_OK) {
        return false;
    }

    m_cache.Set(bundle.name, out);

    if (num_compiled_permutations != 0) {
        DebugLog(
            LogType::Info,
            "Compiled %llu new variants for shader %s to: %s\n",
            num_compiled_permutations,
            bundle.name.Data(),
            final_output_path.Data()
        );
    }

    return true;
}

CompiledShader ShaderCompiler::GetCompiledShader(
    const String &name
)
{
    ShaderProps props { };
    GetDefaultVersions(props);

    return GetCompiledShader(name, props);
}

CompiledShader ShaderCompiler::GetCompiledShader(
    const String &name,
    const ShaderProps &versions
)
{
    CompiledShader compiled_shader;
    GetCompiledShader(name, versions, compiled_shader);

    return compiled_shader;
}

bool ShaderCompiler::GetCompiledShader(
    const String &name,
    const ShaderProps &versions,
    CompiledShader &out
)
{
    const auto version_hash = versions.GetHashCode();
    
    if (m_cache.GetShaderInstance(name, version_hash.Value(), out)) {
        return true;
    }

    CompiledShaderBatch batch;
    if (!LoadOrCreateCompiledShaderBatch(name, versions, batch)) {
        DebugLog(
            LogType::Error,
            "Failed to attempt loading of shader batch\n"
        );

        return false;
    }

    // set in cache so we can use it later
    m_cache.Set(name, batch);

    auto it = batch.compiled_shaders.FindIf([version_hash](const CompiledShader &compiled_shader) {
        return compiled_shader.version_hash == version_hash.Value();
    });

    // what?
    AssertThrow(it != batch.compiled_shaders.End());

    out = *it;

    return true;
}

} // namespace hyperion::v2
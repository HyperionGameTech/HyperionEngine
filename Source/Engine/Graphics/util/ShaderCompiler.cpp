/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/util/ShaderCompiler.hpp>
#include <rendering/util/ShaderPropertyDictionary.hpp>
#include <rendering/util/ShaderCompiler/ShaderCompilerInternal.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Shader.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/GenericPipelineCache.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <Core/json/JSON.hpp>

#include <Core/utilities/ByteUtil.hpp>
#include <Core/utilities/ForEach.hpp>
#include <Core/utilities/Time.hpp>

#include <Core/reflection/Enum.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/io/ByteWriter.hpp>
#include <Core/io/BufferedByteReader.hpp>

#include <Core/math/MathUtil.hpp>

#include <util/ini/INIFile.hpp>

#include <system/DirectoryInitializer.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>

#if HYP_DXC
#if HYP_WINDOWS
#include <Unknwn.h>
#include <d3d12shader.h>
#endif

#include <dxcapi.h>
#endif

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#endif

#include <HyperionEngine.hpp>

#include <ShaderCompiler.generated.inl>

namespace Hyperion {

HYP_DEFINE_LOG_SUBCHANNEL(ShaderCompiler, Core);

// #define HYP_SHADER_COMPILER_LOGGING
#define HYP_ENABLE_SHADER_DEBUGGING

/// Should missing shader variants be compiled when requested, or should we just fail?
/// Enabling this will cause shader compilation to happen during gameplay / editor.
CVar<bool> cvShouldCompileMissingVariants { "ShaderCompiler.CompileMissingVariants", false };

#if HYP_DXC
static IDxcUtils* s_dxcUtils = nullptr;
static IDxcCompiler3* s_dxcCompiler = nullptr;
#endif // HYP_DXC

static constexpr uint32 NumPrecompileShadersThreads = 8;

static const TMap<VertexType, Array<const char*>> s_vertexTypeToVertexAttributes = {
    { VT_Position, { "a_position" } },
    { VT_Normal, { "a_normal" } },
    { VT_UV0, { "a_texcoord0" } },
    { VT_UV1, { "a_texcoord1" } },
    { VT_Skeletal, { "a_bone_indices", "a_bone_weights" } }
};

class PrecompileShadersWorkerPool : public TaskThreadPool
{
public:
    PrecompileShadersWorkerPool()
        : TaskThreadPool(TypeWrapper<TaskThread>(), "PrecompileShadersWorker", NumPrecompileShadersThreads)
    {
    }

    virtual ~PrecompileShadersWorkerPool() override = default;
};

PrecompileShadersWorkerPool* s_precompileShadersPool;

#pragma region Helpers

static const FilePath& GetShaderSourceDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Source/Shaders"), /* RelativeToExecutablePath */ false> s_directory;
    return s_directory.path;
}

#if HYP_DXC
static LPCWSTR GetDXCTargetProfile(ShaderModuleType type)
{
    switch (type)
    {
    case ShaderModuleType::Vertex:
        return L"vs_6_1"; // need 6.1 for multiple views
    case ShaderModuleType::Pixel:
        return L"ps_6_0";
    case ShaderModuleType::Geometry:
        return L"gs_6_0";
    case ShaderModuleType::Compute:
        return L"cs_6_0";
    case ShaderModuleType::RayGen:
    case ShaderModuleType::Miss:
    case ShaderModuleType::ClosestHit:
    case ShaderModuleType::AnyHit:
    case ShaderModuleType::Intersect:
        return L"lib_6_5";
    default:
        return L"vs_6_0";
    }
}
#endif // HYP_DXC

static String InputLayoutToString(const VertexInputLayoutDesc& inputLayout)
{
    String str;

    uint8 mask = inputLayout.mask;

    FOR_EACH_BIT(mask, bit)
    {
        str += VertexUtils::ToString(VertexType(1 << bit));

        mask &= ~(1 << bit);

        if (mask)
        {
            str += ", ";
        }
    }

    return str;
}

String GetShaderVersionFromSource(const String& source, String& outSourceWithoutVersion)
{
    outSourceWithoutVersion = source;

    String sourceTrimmed = source.TrimmedLeft();

    if (sourceTrimmed.StartsWith("#version"))
    {
        size_t firstNewline = sourceTrimmed.FindFirstIndex('\n');
        String versionLine = sourceTrimmed.Substr(0, firstNewline);

        outSourceWithoutVersion = sourceTrimmed.Substr(firstNewline + 1);

        return versionLine.TrimmedRight();
    }

    return "#version 450";
}

static String BuildDescriptorTableDefines(const ShaderInputGroup& inputGroup, ShaderCompileTargetBackend targetBackend = ShaderCompileTargetBackend::Vulkan)
{
    String descriptorTableDefines;

    // Generate descriptor table defines
    for (const ShaderInputSet& inputSet : inputGroup.elements)
    {
        const ShaderInputSet* descriptorSetDeclarationPtr = &inputSet;

        const uint32 setIndex = inputGroup.GetDescriptorSetIndex(inputSet.name);
        Assert(setIndex != -1);

        descriptorTableDefines += "#define _" + String(*inputSet.name) + "_SPACE" + " " + ("space" + String::ToString(setIndex)) + "\n";

        if (inputSet.flags[ShaderInputSetFlags::Reference])
        {
            const ShaderInputSet* referencedDescriptorSetDeclaration = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(inputSet.name);
            Assert(referencedDescriptorSetDeclaration != nullptr);

            descriptorSetDeclarationPtr = referencedDescriptorSetDeclaration;
        }

        for (const Array<ShaderInput>& descriptorDeclarations : descriptorSetDeclarationPtr->slots)
        {
            for (const ShaderInput& shaderInput : descriptorDeclarations)
            {
                descriptorTableDefines += '\t';

                char registerKey = 0;

                switch (shaderInput.slot)
                {
                case ShaderRegister::SRV: // read-only storage buffers and textures
                    registerKey = 't';
                    break;
                case ShaderRegister::UAV: // r/w storage buffers and images
                    registerKey = 'u';
                    break;
                case ShaderRegister::BUFFER: // constant buffers
                    registerKey = 'b';
                    break;
                case ShaderRegister::SAMPLER: // samplers
                    registerKey = 's';
                    break;
                default:
                    HYP_UNREACHABLE();
                }


                const uint32 registerIndex = (targetBackend == ShaderCompileTargetBackend::DX12)
                    ? shaderInput.index  // DX12: use explicit binding index
                    : descriptorSetDeclarationPtr->CalculateFlatIndex(shaderInput.slot, shaderInput.name);  // Vulkan: use flattened index

                descriptorTableDefines += HYP_FORMAT("#define _{}_{}_REGISTER {}{}",
                    descriptorSetDeclarationPtr->name, shaderInput.name,
                    registerKey, registerIndex);

                descriptorTableDefines += '\n';
            }
        }
    }

    return descriptorTableDefines;
}

static String BuildAttributesDefines(const ShaderVariantPerms& perm)
{
    String preamble;

    uint32 attrIndex = 0;

    FOR_EACH_BIT(perm.GetRequiredVertexAttributes().flagMask, bit)
    {
        VertexType vt = VertexType(1 << bit);

        auto it = s_vertexTypeToVertexAttributes.Find(vt);
        Assert(it != s_vertexTypeToVertexAttributes.End());

        const Array<const char*>& vertexAttributes = it->second;
        Assert(vertexAttributes.Any());

        preamble += String("#define VT_") + VertexUtils::ToString(vt) + "\n";

        for (const String& attr : vertexAttributes)
        {
            preamble += HYP_FORMAT("#define HYP_ATTRIBUTE_{}\n", attr);
            preamble += HYP_FORMAT("#define _{}_LOCATION {}\n", attr, attrIndex);

            ++attrIndex;
        }
    }

    // We do not do the same for Optional attributes, as they have not been
    // instantiated at this point in time. before compiling the shader, they
    // should have all been made Required.

    TSet<StringHash> definedNames;

    for (const ShaderProperty& property : perm.GetPropertySet())
    {
        if (!property.name.IsValid())
        {
            continue;
        }

        if (definedNames.Contains(StringHash(property.name)))
        {
            HYP_LOG(ShaderCompiler,
                Warning,
                "Shader property {} defined multiple times in shader properties! This may cause shader compilation errors.",
                property.name);

            continue;
        }

        definedNames.Insert(StringHash(property.name));

        // property has a value -- if integral or float, use that value
        if (property.HasValue())
        {
            if (property.currentValue.Is<Name>())
            {
                // string values are defined as KEY_VALUE = 1
                preamble += HYP_FORMAT("#define {}_{} 1\n", property.name, property.currentValue.Get<Name>());
            }
            else
            {
                preamble += HYP_FORMAT("#define {} {}\n", property.name, property.GetValueString());
            }

            continue;
        }

        // no value set, treat it as boolean true (1)
        preamble += HYP_FORMAT("#define {} 1\n", property.name);
    }

    return preamble;
}

// fallback to allow compiling shaders for vulkan targets when not compiled with vulkan support.
#if !HYP_VULKAN
#ifndef VK_API_VERSION_1_0
#define VK_API_VERSION_1_0 VK_MAKE_API_VERSION(0, 1, 0, 0)
#endif
#ifndef VK_MAKE_API_VERSION
#define VK_MAKE_API_VERSION(variant, major, minor, patch) \
    ((((uint32)(variant)) << 29) | (((uint32)(major)) << 22) | (((uint32)(minor)) << 12) | ((uint32)(patch)))
#endif
#ifndef VK_API_VERSION_1_1
#define VK_API_VERSION_1_1 VK_MAKE_API_VERSION(0, 1, 1, 0)
#endif
#ifndef VK_API_VERSION_1_2
#define VK_API_VERSION_1_2 VK_MAKE_API_VERSION(0, 1, 2, 0)
#endif
#ifndef VK_API_VERSION_1_3
#define VK_API_VERSION_1_3 VK_MAKE_API_VERSION(0, 1, 3, 0)
#endif
#ifndef VK_API_VERSION_1_4
#define VK_API_VERSION_1_4 VK_MAKE_API_VERSION(0, 1, 4, 0)
#endif
static constexpr uint32 HYP_VULKAN_API_VERSION = VK_API_VERSION_1_2;
#endif // !HYP_VULKAN

// Target platform properties for cross-compilation
static const ShaderPropertyId s_propTargetWindows = InternShaderProperty(ShaderProperty(NAME("TARGET"), NAME("WINDOWS")));
static const ShaderPropertyId s_propTargetMac = InternShaderProperty(ShaderProperty(NAME("TARGET"), NAME("MAC")));
static const ShaderPropertyId s_propTargetLinux = InternShaderProperty(ShaderProperty(NAME("TARGET"), NAME("LINUX")));
static const ShaderPropertyId s_propTargetAndroid = InternShaderProperty(ShaderProperty(NAME("TARGET"), NAME("ANDROID")));
static const ShaderPropertyId s_propTargetIOS = InternShaderProperty(ShaderProperty(NAME("TARGET"), NAME("IOS")));

// Target backend properties for cross-compilation
static const ShaderPropertyId s_propVulkan = InternShaderProperty(ShaderProperty(NAME("BACKEND"), NAME("VULKAN")));
static const ShaderPropertyId s_propDX12 = InternShaderProperty(ShaderProperty(NAME("BACKEND"), NAME("DX12")));

static const ShaderPropertyId s_propNumGBufferTextures = InternShaderProperty(ShaderProperty(NAME("NUM_GBUFFER_TEXTURES"), int(NumGBufferTargets)));

static const ShaderPropertyId s_propBindlessTextures = InternShaderProperty(ShaderProperty(NAME("HYP_FEATURES_BINDLESS_TEXTURES")));

static const ShaderPropertyId s_propDebugIrradiance = InternShaderProperty(ShaderProperty(NAME("DEBUG_IRRADIANCE")));
static const ShaderPropertyId s_propDebugVelocity = InternShaderProperty(ShaderProperty(NAME("DEBUG_VELOCITY")));
static const ShaderPropertyId s_propDebugNormals = InternShaderProperty(ShaderProperty(NAME("DEBUG_NORMALS")));
static const ShaderPropertyId s_propDebugAO = InternShaderProperty(ShaderProperty(NAME("DEBUG_AO")));

static String ShaderPropertyValueToString(const ShaderProperty::Value& v)
{
    String str;

    Visit(v, [&](auto&& value)
        {
            str = HYP_FORMAT("{}", value);
        });

    return str;
}

static bool AreShaderPropertyValuesEquivalent(const ShaderProperty::Value& a, const ShaderProperty::Value& b)
{
    if (a.Is<int>() && b.Is<int>())
    {
        return a.GetUnchecked<int>() == b.GetUnchecked<int>();
    }

    if (a.Is<float>() && b.Is<float>())
    {
        return a.GetUnchecked<float>() == b.GetUnchecked<float>();
    }

    if (a.Is<int>() && b.Is<float>())
    {
        const int intVal = a.GetUnchecked<int>();
        const float floatVal = b.GetUnchecked<float>();
        return float(intVal) == floatVal && MathUtil::Fract(floatVal) == 0.0f;
    }

    if (a.Is<float>() && b.Is<int>())
    {
        const float floatVal = a.GetUnchecked<float>();
        const int intVal = b.GetUnchecked<int>();
        return floatVal == float(intVal) && MathUtil::Fract(floatVal) == 0.0f;
    }

    if (a.Is<Name>() && b.Is<Name>())
    {
        return a.GetUnchecked<Name>() == b.GetUnchecked<Name>();
    }

    return false;
}

static void MergeGlobalShaderProperties(bool isPrecompilingShaders, ShaderPropertySet& out);

void MergeGlobalShaderProperties(ShaderPropertySet& out)
{
    MergeGlobalShaderProperties(/* isPrecompilingShaders */ false, out);
}

static void MergeGlobalShaderProperties(bool isPrecompilingShaders, ShaderPropertySet& out)
{
    // Num GBuffer textures is always static
    out.Add(s_propNumGBufferTextures);

    if (isPrecompilingShaders)
    {
        return;
    }

    // Current platform + Graphics API

#if defined(HYP_DX12)
    out.Add(s_propDX12);
#elif defined(HYP_VULKAN)
    out.Add(s_propVulkan);
#endif // HYP_DX12 || HYP_VULKAN

#if defined(HYP_WINDOWS)
    out.Add(s_propTargetWindows);
#elif defined(HYP_MACOS)
    out.Add(s_propTargetMac);
#elif defined(HYP_LINUX)
    out.Add(s_propTargetLinux);
#elif defined(HYP_ANDROID)
    out.Add(s_propTargetAndroid);
#elif defined(HYP_IOS)
    out.Add(s_propTargetIOS);
#endif // HYP_WINDOWS || HYP_MACOS || HYP_LINUX || HYP_ANDROID || HYP_IOS

    if (RI.GetRenderConfig().bindlessTextures)
    {
        out.Add(s_propBindlessTextures);
    }
}

static void MergeGlobalShaderProperties(bool isPrecompilingShaders, ShaderVariantPerms& inOutPerm)
{
    ShaderPropertySet props;
    MergeGlobalShaderProperties(isPrecompilingShaders, props);

    if (isPrecompilingShaders)
    {
        // if compiling the entire bundle (like with PrecompileShaders.exe),
        // we want to add some of these properties as a permutation, rather than as a static property.

        inOutPerm.Set(NAME("HYP_FEATURES_BINDLESS_TEXTURES"), true, SPF_PERMUTATION);

        props.Set(s_propBindlessTextures, false);
    }

    for (const ShaderPropertyId& propertyId : props.ToArray())
    {
        ShaderProperty property;
        if (!GetShaderPropertyById(propertyId, property))
        {
            HYP_LOG(ShaderCompiler, Warning,
                "Failed to get global shader property for id {} when merging global shader properties.",
                uint32(propertyId));

            continue;
        }

        inOutPerm.Set(property);
    }
}

static bool SatisfiesRequested(
    const ShaderPropertySet& requestedProperties,
    const VertexInputLayoutDesc& requestedInputLayout,
    const Shader& candidate,
    bool matchAllProperties)
{
    return candidate.inputLayout == requestedInputLayout
        && (matchAllProperties ? candidate.properties == requestedProperties
                               : (requestedProperties & candidate.properties) == candidate.properties);
}

#pragma endregion Helpers

#pragma region DescriptorUsageSet

void DescriptorUsageSet::BuildDescriptorTableDeclaration(ShaderInputGroup& table) const
{
    for (const DescriptorUsage& descriptorUsage : elements)
    {
        Assert(descriptorUsage.slot != ShaderRegister::NONE && descriptorUsage.slot < ShaderRegister::MAX,
            "Descriptor usage {} has invalid slot {}",
            descriptorUsage.descriptorName.LookupString(), descriptorUsage.slot);

        ShaderInputSet* inputSet = table.FindDescriptorSetDeclaration(descriptorUsage.setName);

        // check if this descriptor set is defined in the static descriptor table
        // if it is, we can use those definitions
        // otherwise, it is a 'custom' descriptor set
        ShaderInputSet* staticDescriptorSetDeclaration = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(descriptorUsage.setName);

        if (staticDescriptorSetDeclaration != nullptr)
        {
            Assert(staticDescriptorSetDeclaration->FindDescriptorDeclaration(descriptorUsage.descriptorName) != nullptr,
                "Descriptor set {} is defined in the static descriptor table, but "
                "the descriptor {} is not",
                descriptorUsage.setName, descriptorUsage.descriptorName);

            if (!inputSet)
            {
                const uint32 setIndex = uint32(table.elements.Size());

                ShaderInputSet newDescriptorSetDeclaration(setIndex, staticDescriptorSetDeclaration->name);
                newDescriptorSetDeclaration.flags = staticDescriptorSetDeclaration->flags | ShaderInputSetFlags::Reference;

                table.AddDescriptorSetDeclaration(std::move(newDescriptorSetDeclaration));
            }

            continue;
        }

        if (!inputSet)
        {
            const uint32 setIndex = uint32(table.elements.Size());

            inputSet = table.AddDescriptorSetDeclaration(ShaderInputSet(setIndex, descriptorUsage.setName));
        }

        AssertDebug(descriptorUsage.category != ShaderResourceCategory::Unknown);

        ShaderInput shaderInput {};
        shaderInput.slot = descriptorUsage.slot;
        shaderInput.type = descriptorUsage.type;
        shaderInput.category = descriptorUsage.category;
        shaderInput.name = descriptorUsage.descriptorName;
        shaderInput.count = descriptorUsage.GetCount();
        shaderInput.size = descriptorUsage.GetSize();
        shaderInput.isDynamic = bool(descriptorUsage.flags & DescriptorUsageFlags::DYNAMIC);
        shaderInput.bufferType = descriptorUsage.bufferType;

        if (auto* existingDecl = inputSet->FindDescriptorDeclaration(descriptorUsage.descriptorName))
        {
            // Already exists, just update the slot
            *existingDecl = std::move(shaderInput);
        }
        else
        {
            inputSet->Add(std::move(shaderInput));
        }
    }
}

#pragma endregion DescriptorUsageSet

#pragma region SPRIV Compilation

static void GetSPIRVEnvironmentInfo(
    ShaderModuleType type,
    uint32& outSpirvVersion,
    uint32& outVulkanVersion)
{
    outSpirvVersion = 450;

    outVulkanVersion = HYP_VULKAN_API_VERSION;

    if (IsRayTracingShaderModule(type))
    {
        outSpirvVersion = MathUtil::Max(outSpirvVersion, 460);

        outVulkanVersion = MathUtil::Max(outVulkanVersion, VK_API_VERSION_1_2);
    }
}

#if HYP_DXC

static bool PreprocessHLSL(
    ShaderModuleType type,
    const String& preamble,
    const String& source,
    const String& filename,
    String& outPreprocessedSource,
    Array<String>& outErrorMessages)
{
    Assert(s_dxcCompiler && s_dxcUtils);

    outPreprocessedSource = source;

    WideString includeDirs[] = {
        WideString(FilePath(filename).BasePath()),
        WideString(GetShaderSourceDirectory()),
        WideString(GetShaderSourceDirectory() / "include")
    };

    Array<LPCWSTR> args;

    for (const WideString& str : includeDirs)
    {
        args.PushBack(L"-I");
        args.PushBack(*str);
    }

    args.PushBack(L"-P");

    String fullSource = preamble + "\n" + source;

    HRESULT hr;

    IDxcBlobEncoding* pSource = nullptr;
    hr = s_dxcUtils->CreateBlobFromPinned(fullSource.Data(), (uint32)fullSource.Size(), CP_UTF8, &pSource);
    HYP_DEFER({ if (pSource) pSource->Release(); });

    if (FAILED(hr))
    {
        outErrorMessages.PushBack(HYP_FORMAT("Failed to create blob! HRESULT: {}", hr));

        return false;
    }

    DxcBuffer sourceBuffer = { pSource->GetBufferPointer(), pSource->GetBufferSize(), 0 };

    IDxcResult* pResult = nullptr;
    HYP_DEFER({ if (pResult) pResult->Release(); });

    IDxcIncludeHandler* pIncludeHandler = nullptr;
    hr = s_dxcUtils->CreateDefaultIncludeHandler(&pIncludeHandler);
    HYP_DEFER({ if (pIncludeHandler) pIncludeHandler->Release(); });

    if (FAILED(hr))
    {
        outErrorMessages.PushBack(HYP_FORMAT("Failed to create default include handler! HRESULT: {}", hr));

        return false;
    }

    hr = s_dxcCompiler->Compile(
        &sourceBuffer,
        args.Data(),
        (uint32)args.Size(),
        pIncludeHandler,
        IID_PPV_ARGS(&pResult));

    IDxcBlobUtf8* pErrors = nullptr;
    pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    HYP_DEFER({ if (pErrors) pErrors->Release(); });

    if (pErrors && pErrors->GetStringLength() > 0)
        outErrorMessages.PushBack(String(pErrors->GetStringPointer()));

    HRESULT status;
    pResult->GetStatus(&status);

    if (FAILED(status))
        return false;

    IDxcBlobUtf8* pOutput = nullptr;
    pResult->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&pOutput), nullptr);
    HYP_DEFER({ if (pOutput) pOutput->Release(); });

    if (pOutput)
    {
        outPreprocessedSource = String(pOutput->GetStringPointer());
        return true;
    }

    return false;
}

enum class HLSLOutputType
{
    SPIRV,
    DXIL
};

static ByteBuffer CompileHLSL(
    ShaderModuleType type,
    HLSLOutputType outputType,
    DescriptorUsageSet& descriptorUsages,
    String source, String filename,
    const ShaderVariantPerms& perm,
    ShaderCompileTargetBackend targetBackend,
    Array<String>& errorMessages)
{
    Assert(s_dxcCompiler && s_dxcUtils);

    ShaderInputGroup inputGroup;
    descriptorUsages.BuildDescriptorTableDeclaration(inputGroup);

    String preamble = BuildDescriptorTableDefines(inputGroup, targetBackend)
        + "\n" + BuildAttributesDefines(perm);

    String fullSource = preamble + "\n" + source;

    IDxcBlobEncoding* pSource = nullptr;
    s_dxcUtils->CreateBlobFromPinned(fullSource.Data(), (uint32)fullSource.Size(), CP_UTF8, &pSource);
    HYP_DEFER({ if (pSource) pSource->Release(); });

    const WideString entryPointName = WideString(DefaultEntryPointNames[uint8(type)]);

    Array<LPCWSTR> args;

    args.PushBack(L"-E");
    args.PushBack(*entryPointName);

    args.PushBack(L"-T");
    args.PushBack(GetDXCTargetProfile(type));

    args.PushBack(L"-HV 2021");

    // enable debug info in HYP_DEBUG_MODE.
#if defined(HYP_DEBUG_MODE) && defined(HYP_ENABLE_SHADER_DEBUGGING)
    args.PushBack(L"-Zi");
    args.PushBack(L"-Od");
#ifdef HYP_DX12
    args.PushBack(L"-Qembed_debug");
#endif // HYP_DX12
#else // !HYP_DEBUG_MODE
    // Optimize that code.
    args.PushBack(L"-O3");
#endif

    if (outputType == HLSLOutputType::SPIRV)
    {
        args.PushBack(L"-spirv");
        // args.PushBack(L"-fvk-use-scalar-layout");
        args.PushBack(L"-fvk-use-dx-layout");

        uint32 spirvVersion;
        uint32 vulkanApiVersion;
        GetSPIRVEnvironmentInfo(type, spirvVersion, vulkanApiVersion);

        switch (vulkanApiVersion)
        {
        case VK_API_VERSION_1_0:
            args.PushBack(L"-fspv-target-env=vulkan1.0");
            break;
        case VK_API_VERSION_1_1:
            args.PushBack(L"-fspv-target-env=vulkan1.1");
            break;
        case VK_API_VERSION_1_2:
            args.PushBack(L"-fspv-target-env=vulkan1.2");
            break;
        case VK_API_VERSION_1_3:
            args.PushBack(L"-fspv-target-env=vulkan1.3");
            break;
        case VK_API_VERSION_1_4:
            args.PushBack(L"-fspv-target-env=vulkan1.4");
            break;
        default:
            errorMessages.PushBack(HYP_FORMAT("Unsupported vulkan version {}", vulkanApiVersion));
            return {};
        }
    }

    DxcBuffer sourceBuffer = { pSource->GetBufferPointer(), pSource->GetBufferSize(), 0 };
    IDxcResult* pResult = nullptr;
    HYP_DEFER({ if (pResult) pResult->Release(); });

    HRESULT res = s_dxcCompiler->Compile(
        &sourceBuffer,
        args.Data(),
        (uint32)args.Size(),
        nullptr,
        IID_PPV_ARGS(&pResult));

    IDxcBlobUtf8* pErrors = nullptr;
    pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    HYP_DEFER({ if (pErrors) pErrors->Release(); });

    if (pErrors && pErrors->GetStringLength() != 0)
    {
        errorMessages.PushBack(String(pErrors->GetStringPointer()));
    }

    pResult->GetStatus(&res);

    if (FAILED(res))
    {
        errorMessages.PushBack(HYP_FORMAT("Failed to compile HLSL shader {}, HRESULT: {}", filename, res));

        return ByteBuffer();
    }

    IDxcBlob* pBlob = nullptr;
    pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pBlob), nullptr);
    HYP_DEFER({ if (pBlob) pBlob->Release(); });

    ByteBuffer bytecode(pBlob->GetBufferSize());
    Assert(bytecode.Size() > 0);

    Memory::Copy(bytecode.Data(), pBlob->GetBufferPointer(), pBlob->GetBufferSize());

    return bytecode;
}

#endif // HYP_DXC

#pragma endregion SPRIV Compilation

static constexpr const char* ShaderLanguageToBinaryExtension(ShaderCompileTargetBackend backend)
{
    switch (backend)
    {
    case ShaderCompileTargetBackend::DX12: return ".dxil";
    case ShaderCompileTargetBackend::Vulkan: return ".spv";
    default: return "";
    }
}

struct LoadedSourceFile
{
    ShaderModuleType type;
    ShaderLanguage language;
    String file;
    Time lastModifiedTimestamp;
    String source;

    FilePath GetOutputFilepath(
        const Shader& shader,
        ShaderCompileTargetBackend backend,
        ShaderCompileTargetPlatform platform) const
    {
        const FilePath path = FilePath(file);

        return GetTempDirectory() / (FilePath(path.StripExtension()).Basename() + "_" + String::ToString(shader.properties.GetHashCode().Value()) + ShaderLanguageToBinaryExtension(backend));
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

static const FlatMap<String, ShaderModuleType> s_shaderTypeNames = {
    { "vs", ShaderModuleType::Vertex },
    { "ps", ShaderModuleType::Pixel },
    { "gs", ShaderModuleType::Geometry },
    { "cs", ShaderModuleType::Compute },
    { "raygen", ShaderModuleType::RayGen },
    { "closesthit", ShaderModuleType::ClosestHit },
    { "anyhit", ShaderModuleType::AnyHit },
    { "miss", ShaderModuleType::Miss },
    { "intersect", ShaderModuleType::Intersect }
};

static VertexType FindVertexType(StringHash name)
{
    if (name == "Position"_sh)
        return VT_Position;

    if (name == "Normal"_sh)
        return VT_Normal;

    if (name == "UV0"_sh)
        return VT_UV0;

    if (name == "UV1"_sh)
        return VT_UV1;

    if (name == "Skeletal"_sh)
        return VT_Skeletal;

    return VT_Invalid;
}

static bool FindVertexType(const UTF8StringView& str, VertexType& outType)
{
    if (str == "a_position")
    {
        outType = VT_Position;
        return true;
    }

    if (str == "a_normal")
    {
        outType = VT_Normal;
        return true;
    }

    if (str == "a_texcoord0")
    {
        outType = VT_UV0;
        return true;
    }

    if (str == "a_texcoord1")
    {
        outType = VT_UV1;
        return true;
    }

    if (str == "a_bone_indices" || str == "a_bone_weights")
    {
        outType = VT_Skeletal;
        return true;
    }

    return false;
}

static VertexTypeMask BuildVertexTypeMask(const Array<VertexAttributeDefinition>& definitions)
{
    VertexTypeMask set {};

    for (const VertexAttributeDefinition& definition : definitions)
    {
        VertexType vt = VT_Invalid;

        if (!FindVertexType(definition.name, vt))
        {
            HYP_LOG(ShaderCompiler, Error, "Invalid vertex attribute definition, {}", definition.name);

            continue;
        }

        set |= vt;
    }

    return set;
}

static bool IsShaderRequestCoveredByPerms(
    const ShaderVariantPerms& bundlePerms,
    const Array<ShaderProperty>& requestedProperties,
    const VertexInputLayoutDesc& requestedInputLayout,
    String* outReason = nullptr)
{
    const bool allowCompileAdditionalVariants = cvShouldCompileMissingVariants.Get();

    for (const ShaderProperty& requested : requestedProperties)
    {
        AssertDebug(!requested.IsPermutable());

        auto bundleIt = bundlePerms.Find(StringHash(requested.name));

        if (bundleIt == bundlePerms.End())
        {
            if (!allowCompileAdditionalVariants)
            {
                // Property not declared in the bundle at all; ignore it silently.
                continue;
            }

            if (outReason)
            {
                *outReason = HYP_FORMAT(
                    "Property '{}' is not declared in the bundle",
                    requested.name);
            }

            return false; // return false, so we end up compiling the new one.

        }

        if (requested.HasValue())
        {
            if (allowCompileAdditionalVariants)
            {
                //ok
                continue;
            }

            if (bundleIt->IsStatic())
            {
                if (AreShaderPropertyValuesEquivalent(bundleIt->currentValue, requested.currentValue))
                {
                    // match, ok
                    continue;
                }

                if (outReason)
                {
                    ShaderProperty dummy(requested.name, requested.currentValue);
                    *outReason = HYP_FORMAT(
                        "Value '{}' for property '{}' does not match the STATIC value '{}'",
                        dummy.GetValueString(),
                        requested.name,
                        ShaderPropertyValueToString(bundleIt->currentValue));
                }

                return false;
            }
            else if (bundleIt->IsValueGroup())
            {
                const bool found = bundleIt->enumValues.FindIf(
                                       [&requested](const ShaderProperty::Value& v)
                                       {
                                           return AreShaderPropertyValuesEquivalent(v, requested.currentValue);
                                       })
                    != bundleIt->enumValues.End();

                if (found)
                {
                    continue;
                }

                if (outReason)
                {
                    ShaderProperty dummy(requested.name, requested.currentValue);
                    *outReason = HYP_FORMAT(
                        "Value '{}' for property '{}' is not a valid value for '{}'.\nApplicable values include: {}",
                        dummy.GetValueString(),
                        requested.name,
                        requested.name,
                        String::Join(bundleIt->enumValues, ", ", &ShaderPropertyValueToString));
                }

                return false;
            }
            else
            {
                HYP_UNREACHABLE();
            }
        }
    }

    const VertexTypeMask allDeclaredAttrs = bundlePerms.GetAllVertexAttributes();

    if ((requestedInputLayout.mask & allDeclaredAttrs.flagMask) != requestedInputLayout.mask)
    {
        if (outReason)
        {
            *outReason = HYP_FORMAT(
                "Requested vertex attributes ({}) are not fully declared in the bundle (declared: {})",
                InputLayoutToString(requestedInputLayout), allDeclaredAttrs.ToString());
        }

        return false;
    }

    return true;
}

static void ForEachPermutation(
    const ShaderVariantPerms& versions,
    const ProcRef<void(const ShaderVariantPerms&)>& callback,
    bool parallel)
{
    Array<ShaderProperty> variableProperties;
    Array<ShaderProperty> staticProperties;
    Array<ShaderProperty> valueGroups;

    uint8 iterMask = 0xFF;
    uint8 attrIndex = 0;

    while (iterMask)
    {
        VertexType vt = VertexType(1 << attrIndex);

        if (versions.HasRequiredVertexAttribute(vt))
        {
            staticProperties.PushBack(ShaderProperty(vt));
        }
        else if (versions.HasOptionalVertexAttribute(vt))
        {
            variableProperties.PushBack(ShaderProperty(vt));
        }

        iterMask &= ~vt;
        ++attrIndex;
    }

    for (const ShaderProperty& property : versions.GetPropertySet())
    {
        if (property.IsValueGroup())
        {
            valueGroups.PushBack(property);
        }
        else if (property.IsPermutable())
        {
            variableProperties.PushBack(property);
        }
        else
        {
            staticProperties.PushBack(property);
        }
    }

    const size_t numPermutations = 1ull << variableProperties.Size();

    Array<ShaderVariantPerms> propertiesBeforeValueGroups;
    Array<ShaderVariantPerms>* currentCombinations = &propertiesBeforeValueGroups;

    for (size_t i = 0; i < numPermutations; i++)
    {
        TSet<ShaderProperty> currentProperties;
        currentProperties.Reserve(ByteUtil::BitCount(i) + staticProperties.Size());
        currentProperties.Merge(staticProperties);

        for (size_t j = 0; j < variableProperties.Size(); j++)
        {
            if (i & (1ull << j))
            {
                AssertDebug(!variableProperties[j].IsValueGroup());

                ShaderProperty newProperty = variableProperties[j];
                ((uint8&)newProperty.flags) &= ~SPF_PERMUTATION; // have to make sure it is not a permutable property anymore.

                currentProperties.Add(newProperty);
            }
        }

        currentCombinations->EmplaceBack(std::move(currentProperties));
    }

    Array<ShaderVariantPerms> propertiesWithValueGroupsApplied;

    if (valueGroups.Any())
    {
        propertiesWithValueGroupsApplied = propertiesBeforeValueGroups;

        // the index where value groups begin.
        const size_t valueGroupsStart = propertiesBeforeValueGroups.Size();

        currentCombinations = &propertiesWithValueGroupsApplied;

        // now apply the value groups onto it
        for (const ShaderProperty& valueGroup : valueGroups)
        {
            Array<ShaderVariantPerms> currentGroupPerms;
            currentGroupPerms.Resize(valueGroup.enumValues.Size() * currentCombinations->Size());

            for (size_t existingCombinationIndex = 0; existingCombinationIndex < currentCombinations->Size(); existingCombinationIndex++)
            {
                for (size_t valueIndex = 0; valueIndex < valueGroup.enumValues.Size(); valueIndex++)
                {
                    // copy the current version of the array
                    ShaderVariantPerms merged = (*currentCombinations)[existingCombinationIndex];

                    AssertDebug(!merged.Has(valueGroup.name), "Duplicate shader property name detected for {}! This will cause shader compilation errors", valueGroup.name);

                    const ShaderProperty::Value& valueAtIndex = valueGroup.enumValues[valueIndex];

                    merged.Set(ShaderProperty(valueGroup.name, valueAtIndex));

                    currentGroupPerms[existingCombinationIndex + (valueIndex * currentCombinations->Size())] = std::move(merged);
                }
            }

#ifdef HYP_SHADER_COMPILER_LOGGING
            HYP_LOG(ShaderCompiler, Info,
                "\tShader value group {} has {} permutations:", valueGroup.name,
                currentGroupPerms.Size());

            for (const ShaderVariantPerms& perm : currentGroupPerms)
            {
                HYP_LOG(ShaderCompiler, Verbose, "\t\t{}", perm.ToString());
            }
#endif

            *currentCombinations = std::move(currentGroupPerms);
        }
    }

#ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(ShaderCompiler, Info,
        "Processing {} shader permutations:", currentCombinations->Size());
#endif

    if (parallel)
    {
        auto CallbackWrapper = [&callback](const ShaderVariantPerms& perm, uint32)
        {
            callback(perm);
        };

        if (s_precompileShadersPool)
        {
            TaskSystem::GetInstance().ParallelForEach(*s_precompileShadersPool, *currentCombinations, CallbackWrapper);
        }
        else
        {
            TaskSystem::GetInstance().ParallelForEach(*currentCombinations, CallbackWrapper);
        }
    }
    else
    {
        for (const ShaderVariantPerms& perm : *currentCombinations)
        {
            callback(perm);
        }
    }
}

static bool LoadBundleFromAssetPath(const AssetPath& path, Handle<ShaderBundle>& outBundle)
{
    Handle<AssetObject> asset = GetEngineAssetRegistry()->GetAsset(path.GetBucket(), path.assetName);

    if (!asset.IsValid() || !asset->IsA(ShaderBundle::StaticClass()))
    {
        HYP_LOG(ShaderCompiler, Warning, "Got invalid ShaderBundle asset at : {} ", path.ToString());
        return false;
    }

    outBundle = DynamicCast<ShaderBundle>(asset);

    return true;
}

#pragma region ShaderProperty

HashCode ShaderProperty::GetHashCode() const
{
    HashCode hc = name.GetHashCode();

    if (HasValue())
    {
        const HashCode valueHashCode = GetValueString().GetHashCode();
        AssertDebug(valueHashCode.Value() != 0);

        hc.Add(valueHashCode);
    }

    // @NOTE: enum values aren't part of the hash code in order to allow
    // changing selecting shader via name / value hash

    return hc;
}

String ShaderProperty::ToString() const
{
    if (IsValueGroup())
    {
        return HYP_FORMAT("{}({})", name, enumValues.Size());
    }
    else if (IsPermutable())
    {
        return HYP_FORMAT("{}(*)", name);
    }
    else if (IsStatic())
    {
        if (HasValue())
        {
            return HYP_FORMAT("{}={}", name, GetValueString());
        }

        return *name;
    }

    HYP_UNREACHABLE();
}

String ShaderProperty::GetValueString() const
{
    if (HasValue())
    {
        return ShaderPropertyValueToString(currentValue);
    }

    return String::empty;
}

#pragma endregion ShaderProperty

#pragma region ShaderVariantPerms

ShaderVariantPerms& ShaderVariantPerms::Set(const ShaderProperty& property, bool enabled)
{
    if (property.IsVertexAttribute())
    {
        VertexType vt = FindVertexType(property.currentValue.Is<Name>()
            ? property.currentValue.GetUnchecked<Name>()
            : Name::Invalid());

        if (vt == VT_Invalid)
        {
            HYP_LOG(ShaderCompiler, Error, "Invalid VertexType for shader: {}", property.GetValueString());

            return *this;
        }

        if (property.IsOptionalVertexAttribute())
        {
            if (enabled)
            {
                m_optionalVertexAttributes |= vt;
                m_optionalVertexAttributes &= ~m_requiredVertexAttributes;
            }
            else
            {
                m_optionalVertexAttributes &= ~vt;
            }

            // NOTE: Optional vertex attributes should not trigger any hash code
            // recalculation.

            return *this;
        }

        if (enabled)
        {
            m_requiredVertexAttributes |= vt;
            m_optionalVertexAttributes &= ~vt;
        }
        else
        {
            m_requiredVertexAttributes &= ~vt;
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

String ShaderVariantPerms::ToString() const
{
    String propertiesString;
    int counter = 0;

    for (const ShaderProperty& property : GetPropertySet())
    {
        propertiesString += property.ToString();

        if (counter++ < GetPropertySet().Size() - 1)
        {
            propertiesString += ", ";
        }
    }

    return propertiesString;
}

#pragma endregion ShaderVariantPerms

#pragma region ShaderPropertySet

Array<ShaderPropertyId> ShaderPropertySet::ToArray() const
{
    Array<ShaderPropertyId> result;
    result.Reserve(ByteUtil::BitCount(chunks[0])
        + ByteUtil::BitCount(chunks[1])
        + ByteUtil::BitCount(chunks[2])
        + ByteUtil::BitCount(chunks[3]));

    uint64 chunkOffset = 0;
    for (uint32 chunk : chunks)
    {
        FOR_EACH_BIT(chunk, bit)
        {
            ShaderPropertyId propertyId = ShaderPropertyId(chunkOffset + bit);

            result.PushBack(propertyId);
        }

        chunkOffset += ShaderPropertySet::ChunkSizeBits;
    }

    return result;
}

String ShaderPropertySet::GetDebugString() const
{
    String str;

    for (ShaderPropertyId propertyId : ToArray())
    {
        if (!str.Empty())
            str += ", ";

        ShaderProperty property;
        if (!GetShaderPropertyById(propertyId, property))
        {
            str += "<UNKNOWN>";
        }

        str += property.ToString();
    }

    return str;
}

#pragma endregion ShaderPropertySet

#pragma region ShaderBundle

Array<Pair<Name, ShaderProperty::Value>> ShaderBundle::SerializeStaticProperties() const
{
    Array<Pair<Name, ShaderProperty::Value>> result;

    for (ShaderPropertyId propertyId : staticProperties)
    {
        ShaderProperty property;
        if (GetShaderPropertyById(propertyId, property))
        {
            result.EmplaceBack(property.name, property.currentValue);
        }
    }

    return result;
}

void ShaderBundle::DeserializeStaticProperties(const Array<Pair<Name, ShaderProperty::Value>>& properties)
{
    for (const Pair<Name, ShaderProperty::Value>& pair : properties)
    {
        ShaderPropertyId propertyId = InternShaderProperty(ShaderProperty(pair.first, pair.second));
        staticProperties.Add(propertyId);
    }
}

HashCode ShaderBundle::GetHashCode() const
{
    HashCode hc;

    for (const Handle<Shader>& shader : compiledShaders)
    {
        hc.Add(shader->GetHashCode());
    }

    hc.Add(staticProperties.GetHashCode());

    return hc;
}

#pragma endregion ShaderBundle

#pragma region ShaderCompiler

ShaderCompiler::ShaderCompiler()
    : m_definitions(nullptr),
      m_isPrecompilingShaders(false)
{
#if HYP_DXC
    if (!s_dxcUtils)
        DxcCreateInstance(CLSID_DxcUtils, __uuidof(IDxcUtils), (void**)&s_dxcUtils);

    if (!s_dxcCompiler)
        DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void**)&s_dxcCompiler);
#endif
}

ShaderCompiler::~ShaderCompiler()
{
#if HYP_DXC
    if (s_dxcUtils)
    {
        s_dxcUtils->Release();
        s_dxcUtils = nullptr;
    }

    if (s_dxcCompiler)
    {
        s_dxcCompiler->Release();
        s_dxcCompiler = nullptr;
    }
#endif

    if (m_definitions)
    {
        delete m_definitions;
    }
}

void ShaderCompiler::ParseDefinitionSection(
    const INIFile::Section& section,
    ShaderBundleDecl& outShaderBundleDecl)
{
    for (const auto& sectionIt : section)
    {
        if (sectionIt.first == "versions")
        {
            // set each property
            for (const auto& element : sectionIt.second.elements)
            {
                if (element.subElements.Any())
                {
                    // Add subelements - parse int / float / string values
                    Array<ShaderProperty::Value> enumValues;
                    enumValues.Reserve(element.subElements.Size());

                    for (const String& subElement : element.subElements)
                    {
                        if (subElement.Empty())
                        {
                            HYP_LOG(ShaderCompiler, Warning,
                                "Empty shader property value for property {}",
                                element.name);

                            continue;
                        }

                        ShaderProperty::Value value;

                        if (std::isdigit(subElement.GetChar(0)))
                        {
                            if (subElement.Contains('.'))
                            {
                                float floatValue;

                                if (!StringUtil::Parse(subElement, &floatValue))
                                {
                                    HYP_LOG(ShaderCompiler, Warning,
                                        "Failed to parse shader property value {} as float for property {}",
                                        subElement, element.name);

                                    continue;
                                }

                                value = floatValue;
                            }
                            else
                            {
                                int intValue;

                                if (!StringUtil::Parse(subElement, &intValue))
                                {
                                    HYP_LOG(ShaderCompiler, Warning,
                                        "Failed to parse shader property value {} as integer for property {}",
                                        subElement, element.name);

                                    continue;
                                }

                                value = intValue;
                            }
                        }
                        else
                        {
                            // string value
                            value = CreateNameFromDynamicString(subElement);
                        }

                        AssertDebug(value.IsValid());

                        enumValues.PushBack(std::move(value));
                    }

                    outShaderBundleDecl.variantPerms.AddValueGroup(CreateNameFromDynamicString(*element.name), enumValues);
                }
                else
                {
                    outShaderBundleDecl.variantPerms.AddPermutation(CreateNameFromDynamicString(*element.name));
                }
            }

            continue;
        }

        auto shaderTypeNameIt = s_shaderTypeNames.Find(sectionIt.first.ToLower());

        if (shaderTypeNameIt != s_shaderTypeNames.End())
        {
            outShaderBundleDecl.sources[shaderTypeNameIt->second] = GetShaderSourceDirectory() / sectionIt.second.GetValue().name;

            continue;
        }

        HYP_LOG(ShaderCompiler, Warning, "Unknown property in shader definition file: {}", sectionIt.first);
    }
}

bool ShaderCompiler::HandleBundle(
    ShaderBundleDecl& decl,
    Optional<ShaderRequest> shaderRequest,
    const Time& lastSavedTimestamp,
    Handle<ShaderBundle>& inOutBundle)
{
    Assert(inOutBundle.IsValid());

    if (CanCompileShaders())
    {
        // Check that each version specified is present in the ShaderBundle.
        // OR any src files have been changed since the object file was compiled.
        // if not, we need to recompile those versions.

        Time maxSourceFileLastModified = Time(0);

        for (const auto& sourceFile : decl.sources)
        {
            maxSourceFileLastModified = MathUtil::Max(maxSourceFileLastModified, FilePath(sourceFile.second).LastModifiedTimestamp());
        }

        if (maxSourceFileLastModified > lastSavedTimestamp)
        {
            HYP_LOG(ShaderCompiler, Verbose,
                "Source file in bundle {} has been modified since the bundle was "
                "last compiled, recompiling...",
                *decl.name);

            return CompileBundle(decl, shaderRequest, inOutBundle);
        }
    }

    bool requestedFound = false;

    if (shaderRequest.HasValue())
    {
        auto requestedIt = inOutBundle->compiledShaders.FindIf([&](const Handle<Shader>& shader)
            {
                return SatisfiesRequested(
                    shaderRequest->properties,
                    shaderRequest->inputLayout,
                    *shader,
                    /* matchAllProperties */ CanCompileShaders());
            });

        requestedFound = requestedIt != inOutBundle->compiledShaders.End();

        if (!requestedFound)
        {
            String requestString = "requested shader with properties: " + shaderRequest->properties.GetDebugString();
            requestString += " and vertex attributes: " + (shaderRequest->inputLayout.mask ? InputLayoutToString(shaderRequest->inputLayout) : "<none>");

            HYP_LOG(ShaderCompiler, Verbose,
                "Bundle {} does not contain a shader satisfying the {}",
                *decl.name, requestString);

            HYP_LOG(ShaderCompiler, Verbose, "Other shaders in the bundle:\n===============================");

            for (const Handle<Shader>& shader : inOutBundle->compiledShaders)
            {
                String shaderString = "\tProperties: " + shader->properties.GetDebugString();
                shaderString += "\n\tVertex attributes: " + (shader->inputLayout.mask ? InputLayoutToString(shader->inputLayout) : "<none>");

                HYP_LOG(ShaderCompiler, Verbose, "{}", shaderString);
            }

            HYP_LOG(ShaderCompiler, Verbose, "===============================");

            if (CanCompileShaders())
            {
                return CompileBundle(decl, shaderRequest, inOutBundle);
            }

            return false;
        }
    }

    return true;
}

bool ShaderCompiler::LoadBundle(
    Name name,
    Optional<ShaderRequest> shaderRequest,
    Handle<ShaderBundle>& outBundle)
{
    outBundle.Reset();

#if HYP_EDITOR
    if (!CanCompileShaders())
    {
        HYP_LOG(ShaderCompiler, Warning,
            "Not compiled with shader compilation support... Shaders may become out of date.\n"
            "If any shader bundle files are missing, they will not be compiled on the fly.");
    }
#endif

    if (!m_definitions || !m_definitions->IsValid())
    {
        // load for first time if no definitions loaded
        if (!LoadShaderDefinitions())
        {
            HYP_LOG(ShaderCompiler, Error, "Failed to load shader definitions");
            return false;
        }
    }

    const String nameString = *name;

    if (!m_definitions->HasSection(nameString))
    {
        // not in definitions file
        HYP_LOG(ShaderCompiler, Error,
            "Section {} not found in shader definitions file", name);

        return false;
    }

    ShaderBundleDecl decl { name };
    MergeGlobalShaderProperties(m_isPrecompilingShaders, decl.variantPerms);

    // apply each permutable property from the definitions file
    const INIFile::Section& section = m_definitions->GetSection(nameString);
    ParseDefinitionSection(section, decl);

    auto ForceRecompile = [&](const AssetPath& path)
    {
        if (CanCompileShaders())
        {
            HYP_LOG(ShaderCompiler, Verbose, "Attempting to compile shader {}...", path.ToString());
        }
        else
        {
            HYP_LOG(ShaderCompiler, Error,
                "Failed to load compiled shader file: {}",
                path.ToString());

            return false;
        }

        if (!CompileBundle(decl, shaderRequest, outBundle))
        {
            HYP_LOG(ShaderCompiler, Error, "Failed to compile shader bundle {}", name);

            return false;
        }

        return LoadBundleFromAssetPath(path, outBundle);
    };

    const AssetPath bundleAssetPath = AssetPath(AssetRegistryId::Engine, AssetBuckets::ShaderBundles, name);

    if (!LoadBundleFromAssetPath(bundleAssetPath, outBundle))
    {
        if (!ForceRecompile(bundleAssetPath))
        {
            HYP_LOG(ShaderCompiler, Error, "Failed to recompile bundle {}", bundleAssetPath.ToString());

            return false;
        }
    }

    Assert(outBundle != nullptr);

    //Handle<AssetPackage> package = outBundle->GetPackage();
    //Assert(package.IsValid());

    //const Time lastSavedTimestamp = package.IsValid() ? package->GetLastSavedTimestamp() : Time(0);

    // @FIXME

    const Time lastSavedTimestamp = Time(0);

    return HandleBundle(decl, shaderRequest, lastSavedTimestamp, outBundle);
}

bool ShaderCompiler::LoadShaderDefinitions(bool precompileShaders, const ShaderCompileParams& params)
{
    // Store the compile params for use during compilation
    m_compileParams = params;

    if (!m_definitions || !m_definitions->IsValid())
    {
        if (m_definitions)
        {
            delete m_definitions;
        }

        m_definitions = new INIFile(GetShaderSourceDirectory() / "Shaders.ini");

        if (!m_definitions->IsValid())
        {
            HYP_LOG(ShaderCompiler, Warning,
                "Failed to load shader definitions file at path: {}",
                m_definitions->GetFilePath());

            delete m_definitions;
            m_definitions = nullptr;

            return false;
        }

        m_shaderBundleDecls.Clear();
        m_shaderBundleDecls.Reserve(m_definitions->GetSections().Size());

        for (const auto& it : m_definitions->GetSections())
        {
            const String& key = it.first;
            const INIFile::Section& section = it.second;

            const Name nameFromString = CreateNameFromDynamicString(ANSIString(key));

            ShaderBundleDecl& decl = m_shaderBundleDecls.EmplaceBack(nameFromString);
            ParseDefinitionSection(section, decl);
        }
    }

    m_isPrecompilingShaders = precompileShaders;

    if (!precompileShaders)
    {
        return true;
    }

    HYP_LOG(ShaderCompiler, Verbose, "Precompiling shaders...");
    HYP_LOG(ShaderCompiler, Info, "Target platforms: {}", EnumToString(params.targetPlatforms));
    HYP_LOG(ShaderCompiler, Info, "Target backends: {}", EnumToString(params.targetBackends));

    if (params.HasShaderFilters())
    {
        HYP_LOG(ShaderCompiler, Info, "Shader filters: {}", String::Join(params.shaderFilters, ", "));
    }

    PrecompileShadersWorkerPool pool;
    s_precompileShadersPool = &pool;

    pool.Start();

    TMap<const ShaderBundleDecl*, bool> results;

    // Compile all shaders ahead of time
    for (const ShaderBundleDecl& decl : m_shaderBundleDecls)
    {
        if (!params.ShaderMatchesFilter(decl.name.ToString()))
        {
            continue;
        }

        Handle<ShaderBundle> bundle;
        if (!LoadBundle(decl.name, Optional<ShaderRequest>(), bundle))
        {
            results[&decl] = false;

            continue;
        }

        results[&decl] = true;
    }

    bool allResults = true;

    for (const auto& it : results)
    {
        if (!it.second)
        {
            String permutationString;

            HYP_LOG(ShaderCompiler, Error,
                "{}: Loading of compiled shader failed!\n\tProperties: {}\n\tAttributes: {}",
                it.first->name, it.first->variantPerms.ToString(), it.first->variantPerms.GetRequiredVertexAttributes().ToString());

            allResults = false;
        }
    }

    m_isPrecompilingShaders = false;

    pool.Stop();

    s_precompileShadersPool = nullptr;

    return allResults;
}

bool ShaderCompiler::CanCompileShaders() const
{
    return CanCompileShaders(m_compileParams);
}

bool ShaderCompiler::CanCompileShaders(const ShaderCompileParams& params) const
{
#if HYP_ANDROID || HYP_IOS
    return false;
#endif

    // Check if we can compile for any of the requested backends
    const bool needsVulkan = params.ShouldCompileVulkan();
    const bool needsDX12 = params.ShouldCompileDX12();

#if HYP_DXC
    // DXC can compile HLSL for both Vulkan (SPIR-V) and DX12 (DXIL)
    if (needsVulkan || needsDX12)
    {
        return true;
    }
#endif

    return false;
}

// Hyperion-specific custom preprocessor directives

static String ExtractFirstToken(const String& str)
{
    String result;

    for (size_t i = 0; i < str.Size(); i++)
    {
        const char ch = str.Data()[i];

        if (std::isalnum(ch) || ch == '_')
        {
            result.Append(ch);
        }
        else
        {
            break;
        }
    }

    return result;
}

static bool MatchesAnyToken(const String& token, std::initializer_list<const char*> keywords)
{
    for (const char* keyword : keywords)
    {
        if (token == keyword)
        {
            return true;
        }
    }

    return false;
}

static TResult<Tuple<ShaderInputType, ShaderResourceCategory, GpuBufferType>> ParseDescriptorTypeFromDeclaration(
    ShaderLanguage language, const String& declaration, EnumFlags<DescriptorUsageFlags> flags)
{
    const String trimmed = declaration.TrimmedLeft();
    const String firstToken = ExtractFirstToken(trimmed);

    auto MakeCBVType = [flags]() -> ShaderInputType
    {
        return (flags & DescriptorUsageFlags::DYNAMIC)
            ? ShaderInputType::CBV_Dynamic
            : ShaderInputType::CBV;
    };

    auto MakeSRVType = [flags]() -> ShaderInputType
    {
        return (flags & DescriptorUsageFlags::DYNAMIC)
            ? ShaderInputType::SRV_Dynamic
            : ShaderInputType::SRV;
    };

    auto MakeUAVType = [flags]() -> ShaderInputType
    {
        return (flags & DescriptorUsageFlags::DYNAMIC)
            ? ShaderInputType::UAV_Dynamic
            : ShaderInputType::UAV;
    };

    if (language == ShaderLanguage::HLSL)
    {
        if (MatchesAnyToken(firstToken, { "cbuffer" }))
        {
            return Tuple<ShaderInputType, ShaderResourceCategory, GpuBufferType> { MakeCBVType(), ShaderResourceCategory::Buffer, GpuBufferType::ConstantBuffer };
        }

        if (MatchesAnyToken(firstToken, { "StructuredBuffer", "ByteAddressBuffer", "Buffer" }))
        {
            GpuBufferType bufferType = GpuBufferType::StructuredBuffer;

            if (firstToken == "ByteAddressBuffer")
            {
                bufferType = GpuBufferType::ByteAddressBuffer;
            }

            return Tuple<ShaderInputType, ShaderResourceCategory, GpuBufferType> { MakeSRVType(), ShaderResourceCategory::Buffer, bufferType };
        }

        if (MatchesAnyToken(firstToken, { "RWStructuredBuffer", "RWByteAddressBuffer", "AppendStructuredBuffer", "ConsumeStructuredBuffer", "RWBuffer" }))
        {
            GpuBufferType bufferType = GpuBufferType::RWStructuredBuffer;
            if (firstToken == "RWByteAddressBuffer")
            {
                bufferType = GpuBufferType::RWByteAddressBuffer;
            }
            return Tuple<ShaderInputType, ShaderResourceCategory, GpuBufferType> { MakeUAVType(), ShaderResourceCategory::Buffer, bufferType };
        }

        if (MatchesAnyToken(firstToken, { "RWTexture1D", "RWTexture2D", "RWTexture3D", "RWTexture1DArray", "RWTexture2DArray" }))
        {
            return Tuple<ShaderInputType, ShaderResourceCategory, GpuBufferType> { ShaderInputType::UAV, ShaderResourceCategory::Image, GpuBufferType::NONE };
        }

        if (MatchesAnyToken(firstToken, { "Texture1D", "Texture2D", "Texture3D", "TextureCube", "Texture1DArray", "Texture2DArray", "TextureCubeArray" }))
        {
            return Tuple<ShaderInputType, ShaderResourceCategory, GpuBufferType> { ShaderInputType::SRV, ShaderResourceCategory::Image, GpuBufferType::NONE };
        }

        if (MatchesAnyToken(firstToken, { "SamplerState", "SamplerComparisonState", "sampler" }))
        {
            return Tuple<ShaderInputType, ShaderResourceCategory, GpuBufferType> { ShaderInputType::Sampler, ShaderResourceCategory::Sampler, GpuBufferType::NONE };
        }

        if (firstToken == "RaytracingAccelerationStructure")
        {
            return Tuple<ShaderInputType, ShaderResourceCategory, GpuBufferType> { ShaderInputType::SRV, ShaderResourceCategory::AccelerationStructure, GpuBufferType::NONE };
        }

        return HYP_MAKE_ERROR(Error, "Unable to determine descriptor type from HLSL declaration: '{}'", trimmed);
    }

    return HYP_MAKE_ERROR(Error, "Unsupported shader language: {}", language);
}

static String FormatDescriptorDeclaration(
    ShaderLanguage language,
    const DescriptorUsage& usage,
    const String& setName,
    const String& descriptorName,
    const String& declarationBody)
{
    String remaining = declarationBody.Trimmed();
    size_t insertPos = remaining.FindFirstIndex(";");

    if (insertPos == String::NotFound)
    {
        insertPos = remaining.FindFirstIndex("{");
    }

    String declaration = (insertPos == String::NotFound) ? remaining : String(remaining.Substr(0, insertPos));
    String suffix = (insertPos == String::NotFound) ? String::empty : String(remaining.Substr(insertPos));

    return HYP_FORMAT("{} : register(_{}_{}_REGISTER, _{}_SPACE) {}\n",
        declaration,
        setName, descriptorName,
        setName,
        suffix);
}

ShaderCompiler::ProcessResult ShaderCompiler::ProcessShaderSource(
    ProcessShaderSourcePhase phase,
    ShaderModuleType type,
    ShaderLanguage language, const String& source, const String& filename,
    const ShaderVariantPerms& perm)
{
    ProcessResult result;
    Array<String> lines;

    if (phase == ProcessShaderSourcePhase::AFTER_PREPROCESS)
    {
        String preprocessedSource;
        Array<String> preprocessErrorMessages;

        bool preprocessResult = false;

        const String preamble = BuildAttributesDefines(perm);

#if HYP_DXC
        preprocessResult = PreprocessHLSL(
            type,
            preamble,
            source,
            filename,
            preprocessedSource,
            preprocessErrorMessages);
#else
        preprocessErrorMessages.PushBack("HLSL preprocessing not supported in this build.");
        preprocessResult = false;
#endif

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

    auto ParseCustomStatement = [](const String& start, const String& line) -> ParseCustomStatementResult
    {
        const String substr = line.Substr(start.Length());

        String argsString;

        int parenthesesDepth = 0;
        size_t index;

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

        return { std::move(args), substr.Substr(index + 1) };
    };

    int lastAttributeLocation = -1;

    for (uint32 lineIndex = 0; lineIndex < lines.Size();)
    {
        const String line = lines[lineIndex++].Trimmed();

        switch (phase)
        {
        case ProcessShaderSourcePhase::BEFORE_PREPROCESS:
        {
            if (type == ShaderModuleType::Vertex && line.StartsWith("HYP_ATTRIBUTE"))
            {
                Array<String> parts = line.Split(' ');

                bool optional = false;

                if (parts.Size() < 3)
                {
                    result.errors.PushBack(ProcessError { "Invalid attribute:  Requires format HYP_ATTRIBUTE type name" });

                    break;
                }

                char ch;

                String attributeKeyword;

                Optional<String> attributeCondition;

                size_t attrStringIndex = 0;

                {
                    while (attrStringIndex != parts.Front().Size() && (std::isalpha(ch = parts.Front()[attrStringIndex]) || ch == '_'))
                    {
                        attributeKeyword.Append(ch);
                        ++attrStringIndex;
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
                }

                const String remaining = line.Substr(attrStringIndex);

                auto IsIdentiferChar = [](utf::Char32 ch)
                {
                    return std::isalnum(utf::Char32(ch)) || ch == utf::Char32('_');
                };

                VertexAttributeDefinition attributeDefinition {};
                attributeDefinition.name = StringUtil::TakeWhile(parts[2], IsIdentiferChar);
                attributeDefinition.typeClass = StringUtil::TakeWhile(parts[1], IsIdentiferChar);

                if (optional)
                {
                    result.optionalAttributes.PushBack(attributeDefinition);

                    if (attributeCondition.HasValue())
                    {
                        result.processedSource += "#if defined(" + attributeCondition.Get() + ") && " + attributeCondition.Get() + "\n";

                        attributeDefinition.condition = *attributeCondition;
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

                // HYP_ATTRIBUTE float3 bitangent : BINORMAL;
                result.processedSource += remaining + '\n';

                if (optional)
                {
                    result.processedSource += "#endif\n";
                }

                continue;
            }

            if (line.StartsWith("PERMUTE") || line.StartsWith("STATIC"))
            {
                String commandStr;

                for (size_t index = 0; index < line.Size(); index++)
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

                if (commandStr != "PERMUTE" && commandStr != "STATIC")
                {
                    break;
                }

                auto parseResult = ParseCustomStatement(commandStr, line);

                if (parseResult.args.Empty() || parseResult.args[0].Empty())
                {
                    result.errors.PushBack(ProcessError { HYP_FORMAT("{}: Expected a property name as the first argument", commandStr) });

                    break;
                }

                const Name propertyName = CreateNameFromDynamicString(ANSIString(parseResult.args[0]));

                auto ParseShaderPropertyValue = [&](const String& valueStr) -> Optional<ShaderProperty::Value>
                {
                    if (valueStr.Empty())
                    {
                        return {};
                    }

                    if (std::isdigit(valueStr.GetChar(0)))
                    {
                        if (valueStr.Contains('.'))
                        {
                            float floatValue;

                            if (!StringUtil::Parse(valueStr, &floatValue))
                            {
                                HYP_LOG(ShaderCompiler, Warning,
                                    "{}: Failed to parse value '{}' as float for property '{}'",
                                    commandStr, valueStr, parseResult.args[0]);

                                return {};
                            }

                            return ShaderProperty::Value(floatValue);
                        }
                        else
                        {
                            int intValue;

                            if (!StringUtil::Parse(valueStr, &intValue))
                            {
                                HYP_LOG(ShaderCompiler, Warning,
                                    "{}: Failed to parse value '{}' as integer for property '{}'",
                                    commandStr, valueStr, parseResult.args[0]);

                                return {};
                            }

                            return ShaderProperty::Value(intValue);
                        }
                    }

                    return ShaderProperty::Value(CreateNameFromDynamicString(ANSIString(valueStr)));
                };

                if (commandStr == "PERMUTE")
                {
                    if (parseResult.args.Size() == 1)
                    {
                        result.scannedProperties.AddPermutation(propertyName);
                    }
                    else
                    {
                        Array<ShaderProperty::Value> enumValues;

                        for (size_t argIndex = 1; argIndex < parseResult.args.Size(); argIndex++)
                        {
                            Optional<ShaderProperty::Value> value = ParseShaderPropertyValue(parseResult.args[argIndex]);

                            if (value.HasValue())
                            {
                                enumValues.PushBack(value.Get());
                            }
                        }

                        result.scannedProperties.AddValueGroup(propertyName, enumValues);
                    }
                }
                else if (commandStr == "STATIC")
                {
                    if (parseResult.args.Size() >= 2)
                    {
                        Optional<ShaderProperty::Value> value = ParseShaderPropertyValue(parseResult.args[1]);

                        if (value.HasValue())
                        {
                            result.scannedProperties.AddStatic(propertyName, value.Get());
                        }
                        else
                        {
                            result.scannedProperties.AddStatic(propertyName);
                        }
                    }
                    else
                    {
                        result.scannedProperties.AddStatic(propertyName);
                    }
                }

                // strip; don't emit anything for the line.
                continue;
            }

            break;
        }
        case ProcessShaderSourcePhase::AFTER_PREPROCESS:
        {
            if (line.StartsWith("DECLARE"))
            {
                String commandStr;

                for (size_t index = 0; index < line.Size(); index++)
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

                ShaderRegister slot = ShaderRegister::NONE;
                EnumFlags<DescriptorUsageFlags> flags = DescriptorUsageFlags::NONE;

                if (commandStr == "DECLARE_SRV" || commandStr == "DECLARE_SRV_DYNAMIC")
                {
                    slot = ShaderRegister::SRV;
                }
                else if (commandStr == "DECLARE_UAV" || commandStr == "DECLARE_UAV_DYNAMIC")
                {
                    slot = ShaderRegister::UAV;
                }
                else if (commandStr == "DECLARE_BUFFER" || commandStr == "DECLARE_BUFFER_DYNAMIC")
                {
                    slot = ShaderRegister::BUFFER;
                }
                else if (commandStr == "DECLARE_ACCELERATION_STRUCTURE")
                {
                    slot = ShaderRegister::SRV;
                }
                else if (commandStr == "DECLARE_SAMPLER")
                {
                    slot = ShaderRegister::SAMPLER;
                }
                else
                {
                    result.errors.PushBack(ProcessError {
                        "Invalid descriptor slot. Must match DECLARE_<Type> " });

                    break;
                }

                if (commandStr.EndsWith("_DYNAMIC"))
                {
                    flags |= DescriptorUsageFlags::DYNAMIC;
                }

                Array<String> parts = line.Split(' ');

                String descriptorName, setName, slotStr;

                TMap<String, String> params;

                auto parseResult = ParseCustomStatement(commandStr, line);

                if (parseResult.args.Size() < 2)
                {
                    result.errors.PushBack(ProcessError {
                        "Invalid descriptor: Requires format "
                        "DECLARE_<SLOT>(set, name)" });

                    break;
                }

                setName = parseResult.args[0];
                descriptorName = parseResult.args[1];

                if (parseResult.args.Size() > 2)
                {
                    for (size_t index = 2; index < parseResult.args.Size(); index++)
                    {
                        Array<String> split = parseResult.args[index].Split('=');

                        for (String& part : split)
                        {
                            part = part.Trimmed();
                        }

                        if (split.Size() != 2)
                        {
                            result.errors.PushBack(ProcessError {
                                "Invalid parameter: Requires format <key>=<value>" });

                            break;
                        }

                        const String& key = split[0];
                        const String& value = split[1];

                        params[key] = value;
                    }
                }

                TResult<Tuple<ShaderInputType, ShaderResourceCategory, GpuBufferType>> descriptorTypeResult = ParseDescriptorTypeFromDeclaration(language, parseResult.remaining, flags);

                if (!descriptorTypeResult)
                {
                    result.errors.PushBack(ProcessError { descriptorTypeResult.GetError().GetMessage() });

                    continue;
                }

                DescriptorUsage usage {};
                usage.slot = slot;
                usage.type = descriptorTypeResult.GetValue().GetElement<0>();
                usage.category = descriptorTypeResult.GetValue().GetElement<1>();
                usage.bufferType = descriptorTypeResult.GetValue().GetElement<2>();
                usage.setName = CreateNameFromDynamicString(ANSIString(setName));
                usage.descriptorName = CreateNameFromDynamicString(ANSIString(descriptorName));
                usage.flags = flags;
                usage.params = std::move(params);

                AssertDebug(usage.category != ShaderResourceCategory::Unknown);

                result.processedSource += FormatDescriptorDeclaration(
                    language, usage, setName, descriptorName, parseResult.remaining);

                result.descriptorUsages.PushBack(usage);

                continue;
            }

            break;
        }
        }

        result.processedSource += line + '\n';
    }

#ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(ShaderCompiler, Verbose, "Processed source: {}", result.processedSource);
#endif

    return result;
}

bool ShaderCompiler::CompileBundle(
    const ShaderBundleDecl& decl,
    Optional<ShaderRequest> shaderRequest,
    Handle<ShaderBundle>& outBundle)
{
    if (!CanCompileShaders())
    {
        return false;
    }

    if (!outBundle.IsValid())
    {
        outBundle = MakeHandle<ShaderBundle>(decl.name);
    }

    Array<LoadedSourceFile> loadedSourceFiles;
    loadedSourceFiles.Resize(decl.sources.Size());

    Array<Array<ProcessError>> processErrors;
    processErrors.Resize(decl.sources.Size());

    Array<Array<VertexAttributeDefinition>> requiredVertexAttributes;
    requiredVertexAttributes.Resize(decl.sources.Size());

    Array<Array<VertexAttributeDefinition>> optionalVertexAttributes;
    optionalVertexAttributes.Resize(decl.sources.Size());

    Array<ShaderVariantPerms> scannedPropertiesPerFile;
    scannedPropertiesPerFile.Resize(decl.sources.Size());

    TaskBatch taskBatch;

    for (size_t index = 0; index < decl.sources.Size(); index++)
    {
        StaticMessage debugName;
        debugName.value = ANSIStringView(*decl.sources.AtIndex(index).second);

        taskBatch.AddTask([this, index, &decl, &loadedSourceFiles, &processErrors,
                              &requiredVertexAttributes,
                              &optionalVertexAttributes,
                              &scannedPropertiesPerFile](...)
            {
                const auto& pair = decl.sources.AtIndex(index);

                const ShaderModuleType moduleType = pair.first;
                const FilePath filepath = pair.second;

                if (!filepath.Exists())
                {
                    processErrors[index] = {
                        ProcessError { "Shader source file does not exist" }
                    };

                    return;
                }

                FileBufferedReaderSource filepathSource { filepath };
                BufferedReader reader { &filepathSource };

                if (!reader.IsOpen())
                {
                    processErrors[index] = { ProcessError { HYP_FORMAT("Failed to open shader source file: {}", std::strerror(errno)) } };

                    return;
                }

                const ByteBuffer byteBuffer = reader.ReadBytes();

                // we add this define to prevent the DECLARE_* macros from being defines in shader code
                // and folding to nothing.
                String preamble = "#define HYP_SHADER_COMPILER 1\n\n"
                    + HYP_FORMAT("#define {} 1\n\n", ShaderModuleTypeNames[uint8(moduleType)])
                    + "#define LANG_HLSL 1\n\n";

                String sourceString = String(byteBuffer.ToByteView()).ReplaceAll("\r\n", "\n");

                preamble += "#line 1\n\n";
                sourceString = preamble + sourceString;

                // process shader source to extract vertex attributes.
                // runs before actual preprocessing
                ProcessResult result = ProcessShaderSource(
                    ProcessShaderSourcePhase::BEFORE_PREPROCESS,
                    pair.first,
                    ShaderLanguage::HLSL,
                    sourceString,
                    filepath,
                    {});

                if (result.errors.Any())
                {
                    HYP_LOG(ShaderCompiler, Error, "{} shader processing errors!", result.errors.Size());

                    processErrors[index] = result.errors;

                    return;
                }

                requiredVertexAttributes[index] = result.requiredAttributes;
                optionalVertexAttributes[index] = result.optionalAttributes;
                scannedPropertiesPerFile[index] = std::move(result.scannedProperties);

                loadedSourceFiles[index] = LoadedSourceFile {
                    .type = pair.first,
                    .language = ShaderLanguage::HLSL,
                    .file = pair.second,
                    .lastModifiedTimestamp = filepath.LastModifiedTimestamp(),
                    .source = result.processedSource
                };
            });
    }

    taskBatch.ExecuteBlocking();

#if 0
    if (IsOnThread(ThreadCategory::THREAD_CATEGORY_TASK))
    {
        // run on this thread if we are already in a task thread
        taskBatch.ExecuteBlocking();
    }
    else
    {
        // Hack fix: task threads that are currently enqueueing RenderCommands can
        // cause a deadlock, if we are waiting on tasks to complete from the render
        // thread.

        if (IsOnThread(g_renderThread))
        {
            taskBatch.ExecuteBlocking();
        }
        else
        {
            TaskSystem::GetInstance().EnqueueBatch(&taskBatch);
            taskBatch.AwaitCompletion();
        }
    }
#endif

    Array<ProcessError> allProcessErrors;

    for (const auto& errorList : processErrors)
    {
        allProcessErrors.Concat(errorList);
    }

    if (!allProcessErrors.Empty())
    {
        for (const ProcessError& error : allProcessErrors)
        {
            HYP_LOG(ShaderCompiler, Error, "\t{}", error.errorMessage);
        }

        return false;
    }

    static const auto MergeProperty = [](ShaderVariantPerms& target, const ShaderProperty& prop) -> Result
    {
        auto targetIt = target.Find(StringHash(prop.name));

        if (prop.HasValue())
        {
            // Find ValueGroup in target with same name
            if (targetIt != target.End())
            {
                if (*targetIt == prop)
                {
                    // already exists but equal, ok.
                    return {};
                }

                if (targetIt->IsValueGroup())
                {
                    // Add each value from prop to the target's ValueGroup
                    targetIt->AddEnumValue(prop.currentValue);

                    return {};
                }

                if (targetIt->IsStatic())
                {
                    // Convert to a ValueGroup.
                    Array<ShaderProperty::Value> valueArray;
                    if (targetIt->HasValue())
                    {
                        valueArray.PushBack(targetIt->currentValue);
                    }
                    valueArray.PushBack(prop.currentValue);

                    target.AddValueGroup(prop.name, valueArray);
                }

                return HYP_MAKE_ERROR(Error, "Duplicate property: {} already exists and cannot be appened to.", prop.name);
            }
            else
            {
                //Array<ShaderProperty::Value> valueArray(1);
                //valueArray[0] = prop.currentValue;

                //// Add new ValueGroup to the shader variant.
                //target.AddValueGroup(prop.name, valueArray);

                target.AddStatic(prop.name, prop.currentValue);

                return {};
            }
        }

        //if (targetIt != target.End() && *targetIt == prop)
        //{
        //    // already exists but equal; no need to add or give duplication error.
        //    return {};
        //}

        if (prop.IsValueGroup())
        {
            if (targetIt != target.End())
            {
                // merge each value from prop to the target's ValueGroup
                if (targetIt->IsValueGroup())
                {
                    for (const ShaderProperty::Value& enumValue : prop.enumValues)
                    {
                        targetIt->AddEnumValue(enumValue);
                    }

                    return {};
                }

                // conflict: trying to add a ValueGroup to a non-ValueGroup property
                return HYP_MAKE_ERROR(Error, "Duplicate property: {} already exists and is not a ValueGroup we can merge with!", prop.name);
            }
            else
            {
                target.AddValueGroup(prop.name, prop.enumValues);

                return {};
            }
        }

        if (prop.IsPermutable())
        {
            target.AddPermutation(prop.name);

            return {};
        }

        target.AddStatic(prop.name, prop.currentValue);

        return {};
    };

    // grab each defined property, and iterate over each combination
    ShaderVariantPerms declaredPerms;
    MergeGlobalShaderProperties(m_isPrecompilingShaders, declaredPerms);

    ShaderVariantPerms permsToCompile = declaredPerms;

    // For precompiling shaders, we allow targetting multiple platforms, not just the current (host) platform
    if (m_isPrecompilingShaders)
    {
        Array<ShaderProperty::Value> platformValues;

        if (m_compileParams.targetPlatforms[ShaderCompileTargetPlatform::Windows])
            platformValues.PushBack(ShaderProperty::Value(NAME("WINDOWS")));

        if (m_compileParams.targetPlatforms[ShaderCompileTargetPlatform::Mac])
            platformValues.PushBack(ShaderProperty::Value(NAME("MAC")));

        if (m_compileParams.targetPlatforms[ShaderCompileTargetPlatform::Linux])
            platformValues.PushBack(ShaderProperty::Value(NAME("LINUX")));

        if (m_compileParams.targetPlatforms[ShaderCompileTargetPlatform::Android])
            platformValues.PushBack(ShaderProperty::Value(NAME("ANDROID")));

        if (m_compileParams.targetPlatforms[ShaderCompileTargetPlatform::iOS])
            platformValues.PushBack(ShaderProperty::Value(NAME("IOS")));

        if (platformValues.Any())
        {
            declaredPerms.AddValueGroup(NAME("TARGET"), platformValues);
        }

        Array<ShaderProperty::Value> backendValues;

        if (m_compileParams.targetBackends[ShaderCompileTargetBackend::Vulkan])
            backendValues.PushBack(ShaderProperty::Value(NAME("VULKAN")));

        if (m_compileParams.targetBackends[ShaderCompileTargetBackend::DX12])
            backendValues.PushBack(ShaderProperty::Value(NAME("DX12")));

        if (backendValues.Any())
        {
            declaredPerms.AddValueGroup(NAME("BACKEND"), backendValues);
        }
    }
    else
    {
        // Only compile for the active platform/backend when not precompiling.

        Name activePlatform;
#if HYP_WINDOWS
        activePlatform = NAME("WINDOWS");
#elif HYP_MACOS
        activePlatform = NAME("MAC");
#elif HYP_LINUX
        activePlatform = NAME("LINUX");
#elif HYP_ANDROID
        activePlatform = NAME("ANDROID");
#elif HYP_IOS
        activePlatform = NAME("IOS");
#endif

        if (activePlatform.IsValid())
        {
            declaredPerms.Set(ShaderProperty(NAME("TARGET"), activePlatform));
        }

        Name graphicsApi;
#if HYP_VULKAN
        graphicsApi = NAME("VULKAN");
#elif HYP_DX12
        graphicsApi = NAME("DX12");
#endif

        if (graphicsApi.IsValid())
        {
            declaredPerms.Set(ShaderProperty(NAME("BACKEND"), graphicsApi));
        }
    }

    outBundle->staticProperties.Clear();

    for (const ShaderProperty& permProperty : decl.variantPerms.GetPropertySet())
    {
        MergeProperty(declaredPerms, permProperty);
    }

    for (const ShaderVariantPerms& scannedPerms : scannedPropertiesPerFile)
    {
        for (const ShaderProperty& permProperty : scannedPerms.GetPropertySet())
        {
            MergeProperty(declaredPerms, permProperty);

            if (permProperty.IsStatic())
            {
                ShaderPropertyId propertyId = InternShaderProperty(permProperty);

                if (outBundle->staticProperties.Contains(propertyId))
                {
                    continue;
                }

                outBundle->staticProperties.Add(propertyId);
            }
        }
    }

    { // Lookup vertex attribute names
        VertexTypeMask requiredVertexTypeMask;
        VertexTypeMask optionalVertexTypeMask;

        for (const Array<VertexAttributeDefinition>& definitions : requiredVertexAttributes)
        {
            requiredVertexTypeMask |= BuildVertexTypeMask(definitions);
        }

        for (const Array<VertexAttributeDefinition>& definitions : optionalVertexAttributes)
        {
            optionalVertexTypeMask |= BuildVertexTypeMask(definitions);
        }

        declaredPerms.SetRequiredVertexAttributes(requiredVertexTypeMask);
        declaredPerms.SetOptionalVertexAttributes(optionalVertexTypeMask);
    }

    if (m_isPrecompilingShaders)
    {
        permsToCompile = declaredPerms;
    }
    else
    {
        permsToCompile.SetRequiredVertexAttributes(declaredPerms.GetRequiredVertexAttributes());
        permsToCompile.SetOptionalVertexAttributes(declaredPerms.GetOptionalVertexAttributes());
    }

    // INFO ON MERGING 'ADDITIONAL' SHADER VERSIONS (upon requesting a shader)
    // ============================================
    // if shaderRequest is set, we need to properly merge those properties with our ShaderVariant.
    //
    // We assume all requested properties are NOT permutable.
    //
    // VALUE GROUPS / ENUMS:
    // If OUTPUT=RGBA8 is added and we have in our ShaderVariant, OUTPUT={RGBA8, RGBA16F}, we need to add
    // RGBA8 to the existing `OUTPUT` ValueGroup rather than applying it to EVERY single permutation.
    // =============================================
    if (shaderRequest.HasValue())
    {
        // add static properties from the bundle
        for (ShaderPropertyId propertyId : outBundle->staticProperties.ToArray())
        {
            shaderRequest->properties.Add(propertyId);
        }

        Array<ShaderProperty> additionalProperties;

        for (ShaderPropertyId propertyId : shaderRequest->properties.ToArray())
        {
            ShaderProperty property;
            if (!GetShaderPropertyById(propertyId, property))
            {
                HYP_LOG(ShaderCompiler, Error, "Failed to find ShaderProperty with ID {} in reverse lookup map!", uint32(propertyId));

                return false;
            }

            AssertDebug(!property.IsPermutable());

            additionalProperties.PushBack(std::move(property));
        }

        String coverageFailReason;
        if (!IsShaderRequestCoveredByPerms(declaredPerms, additionalProperties, shaderRequest->inputLayout, &coverageFailReason))
        {
            if (cvShouldCompileMissingVariants.Get())
            {
                HYP_LOG(ShaderCompiler, Warning,
                    "Shader request for bundle '{}' is not covered by the bundle's declared permutations: {}\n"
                    "Compiling missing variant on the fly because ShaderCompiler.CompileMissingVariants is enabled.",
                    decl.name, coverageFailReason);

                // Fall through, MergeProperty below will add the requested properties to the perm set, and we'll compile a new variant for it.
            }
            else
            {
                HYP_LOG(ShaderCompiler, Error,
                    "Shader request for bundle '{}' is not covered by the bundle's declared permutations: {}\n"
                    "Ensure PERMUTE() / STATIC() declarations that cover the desired property set exist in the shader source",
                    decl.name, coverageFailReason);

                return false;
            }
        }

        for (const ShaderProperty& additionalProperty : additionalProperties)
        {
            if (Result mergeResult = MergeProperty(permsToCompile, additionalProperty); mergeResult.HasError())
            {
                HYP_LOG(ShaderCompiler, Warning,
                    "Failed to merge additional shader property {} into final properties: {}",
                    additionalProperty.name,
                    mergeResult.GetError().GetMessage());
            }
        }
    }

    Mutex compiledShadersMutex;
    Mutex errorMessagesMutex;

    uint32 numCompiledPermutations = 0;
    uint32 numErroredPermutations = 0;

    Array<Handle<Shader>> existingShadersToRemove;
    TSet<Name> usedNames;

#if HYP_ENABLE_SHADER_RELOAD
    Time maxSourceFileLastModified = Time(0);
    for (const LoadedSourceFile& sourceFile : loadedSourceFiles)
    {
        maxSourceFileLastModified = MathUtil::Max(maxSourceFileLastModified, sourceFile.lastModifiedTimestamp);
    }
#endif

    // Helper to extract target backend from permutation
    auto GetTargetBackendFromPerm = [](const ShaderVariantPerms& perm) -> Optional<ShaderCompileTargetBackend>
    {
        auto backendIt = perm.Find("BACKEND"_sh);

        if (backendIt != perm.End() && backendIt->HasValue() && backendIt->currentValue.Is<Name>())
        {
            const Name backendName = backendIt->currentValue.Get<Name>();

            if (backendName == "VULKAN"_sh)
                return ShaderCompileTargetBackend::Vulkan;

            if (backendName == "DX12"_sh)
                return ShaderCompileTargetBackend::DX12;
        }

        return {};
    };

    // Helper to extract target platform from permutation
    auto GetTargetPlatformFromPerm = [](const ShaderVariantPerms& perm) -> Optional<ShaderCompileTargetPlatform>
    {
        auto platformIt = perm.Find("TARGET"_sh);

        if (platformIt != perm.End() && platformIt->HasValue() && platformIt->currentValue.Is<Name>())
        {
            const Name platformName = platformIt->currentValue.Get<Name>();

            if (platformName == "WINDOWS"_sh)
                return ShaderCompileTargetPlatform::Windows;

            if (platformName == "MAC"_sh)
                return ShaderCompileTargetPlatform::Mac;

            if (platformName == "LINUX"_sh)
                return ShaderCompileTargetPlatform::Linux;

            if (platformName == "ANDROID"_sh)
                return ShaderCompileTargetPlatform::Android;

            if (platformName == "IOS"_sh)
                return ShaderCompileTargetPlatform::iOS;
        }

        return {};
    };

    // compile shader with each permutation of properties
    ForEachPermutation(
        permsToCompile,
        [&](const ShaderVariantPerms& perm)
        {
            HYP_LOG(ShaderCompiler, Verbose, "Compiling shader {}\n\tProperties: {}\n\tAttributes: {}",
                decl.name,
                perm.ToString(),
                perm.GetRequiredVertexAttributes().ToString());

            // Get the target backend and platform for this specific permutation
            const Optional<ShaderCompileTargetBackend> targetBackend = GetTargetBackendFromPerm(perm);
            const Optional<ShaderCompileTargetPlatform> targetPlatform = GetTargetPlatformFromPerm(perm);

            if (!targetBackend.HasValue() || !targetPlatform.HasValue())
            {
                HYP_LOG(ShaderCompiler, Warning, "No target backend or platform for shader {}. Target backend: {}, target platform: {}", decl.name,
                    targetBackend.HasValue() ? *EnumToString(*targetBackend) : "<none>",
                    targetPlatform.HasValue() ? *EnumToString(*targetPlatform) : "<none>");
                return;
            }

            // Determine if we're compiling for Vulkan or DX12 for this specific variant
            const bool isVulkan = targetBackend.Get() == ShaderCompileTargetBackend::Vulkan;
            const bool isDX12 = targetBackend.Get() == ShaderCompileTargetBackend::DX12;

            // DX12 is exclusive to Windows; skip invalid platform+backend combinations
            if (isDX12 && targetPlatform.Get() != ShaderCompileTargetPlatform::Windows)
            {
                HYP_LOG(ShaderCompiler, Verbose, "Skipping DX12 shader variant for non-Windows platform: {}", perm.ToString());
                return;
            }

            HashCode permHashCode = perm.GetPropertySetHashCode();
            permHashCode.Add(perm.GetRequiredVertexAttributes().GetHashCode());

            Handle<Shader> shader = MakeHandle<Shader>(NAME_FMT("{}_{}", decl.name, permHashCode.Value()));
            shader->baseName = decl.name;

            shader->inputLayout = { perm.GetRequiredVertexAttributes().flagMask };

            uint32 numErrored = 0;
            uint32 numCompiled = 0;

            Array<DescriptorUsageSet> descriptorUsageSetsPerFile;
            descriptorUsageSetsPerFile.Resize(loadedSourceFiles.Size());

            Array<String> processedSources;
            processedSources.Resize(loadedSourceFiles.Size());

            Array<Pair<FilePath, bool /* skip */>> filepaths;
            filepaths.Resize(loadedSourceFiles.Size());

            // load each source file, check if the output file exists, and if it
            // does, check if it is older than the source file if it is, we can
            // reuse the file. otherwise, we process the source and prepare it for
            // compilation
            for (size_t index = 0; index < loadedSourceFiles.Size(); index++)
            {
                const LoadedSourceFile& item = loadedSourceFiles[index];

                // check if a file exists w/ same hash
                const FilePath outputFilepath = item.GetOutputFilepath(*shader, *targetBackend, *targetPlatform);

                filepaths[index] = { outputFilepath, false };

                DescriptorUsageSet& descriptorUsages = descriptorUsageSetsPerFile[index];

                Array<String> errorMessages;

                // set directory to the directory of the shader
                const FilePath dir = GetShaderSourceDirectory() / FilePath::Relative(FilePath(item.file).BasePath(), GetShaderSourceDirectory());

                String& processedSource = processedSources[index];

                { // Process shader (preprocessing, custom statements, etc.)
                    ProcessResult processResult = ProcessShaderSource(
                        ProcessShaderSourcePhase::AFTER_PREPROCESS,
                        item.type,
                        item.language,
                        item.source,
                        item.file,
                        perm);

                    if (processResult.errors.Any())
                    {
                        HYP_LOG(ShaderCompiler, Error, "{} shader processing errors:", processResult.errors.Size());

                        for (const ProcessError& processError : processResult.errors)
                        {
                            HYP_LOG(ShaderCompiler, Error, "\t{}", processError.errorMessage);
                        }

                        Mutex::Guard guard(errorMessagesMutex);
                        outBundle->errorMessages.Concat(Map(processResult.errors, &ProcessError::errorMessage));

                        ++numErrored;

                        continue;
                    }

                    descriptorUsages.Merge(std::move(processResult.descriptorUsages));

                    processedSource = processResult.processedSource;
                }
            }

            // merge all descriptor usages together for the source files before
            // compiling.
            DescriptorUsageSet descriptorUsageSetsMerged;

            for (const DescriptorUsageSet& descriptorUsageSet : descriptorUsageSetsPerFile)
            {
                descriptorUsageSetsMerged.Merge(descriptorUsageSet);
            }

            descriptorUsageSetsPerFile.Clear();

            // for debug logging.
            String staticPropertiesString;

            for (const ShaderProperty& property : perm.ToArray())
            {
                if (!staticPropertiesString.Empty())
                {
                    staticPropertiesString += ", ";
                }

                staticPropertiesString += property.ToString();
            }

            HYP_LOG(
                ShaderCompiler,
                Verbose,
                "Compiling shader {}\n\tProperties: [{}]",
                decl.name,
                staticPropertiesString);

            // final substitution of properties + compilation
            for (size_t index = 0; index < loadedSourceFiles.Size(); index++)
            {
                const LoadedSourceFile& item = loadedSourceFiles[index];

                const Pair<FilePath, bool>& filepathState = filepaths[index];

                // don't process these files
                if (filepathState.second)
                {
                    continue;
                }

                const FilePath& outputFilepath = filepathState.first;

                Array<String> errorMessages;

                ByteBuffer byteBuffer;

#if HYP_DXC
                HLSLOutputType outputType;
                ShaderCompileTargetBackend hlslTargetBackend;

                if (isDX12)
                {
                    outputType = HLSLOutputType::DXIL;
                    hlslTargetBackend = ShaderCompileTargetBackend::DX12;
                }
                else if (isVulkan)
                {
                    outputType = HLSLOutputType::SPIRV;
                    hlslTargetBackend = ShaderCompileTargetBackend::Vulkan;
                }
                else
                {
                    Mutex::Guard guard(errorMessagesMutex);
                    outBundle->errorMessages.EmplaceBack("Cannot determine HLSL output type - no target backend specified for this variant");

                    ++numErrored;

                    continue;
                }

                byteBuffer = CompileHLSL(
                    item.type,
                    outputType,
                    descriptorUsageSetsMerged,
                    processedSources[index],
                    item.file,
                    perm,
                    hlslTargetBackend,
                    errorMessages);

                if (errorMessages.Any())
                {
                    Mutex::Guard guard(errorMessagesMutex);
                    outBundle->errorMessages.Concat(errorMessages);

                    ++numErrored;

                    continue;
                }

#else
                Mutex::Guard guard(errorMessagesMutex);
                outBundle->errorMessages.EmplaceBack("Cannot compile HLSL code, DXC not linked");

                ++numErrored;

                continue;
#endif

                if (byteBuffer.Empty())
                {
                    Mutex::Guard guard(errorMessagesMutex);
                    outBundle->errorMessages.EmplaceBack("No shader IL returned");

                    ++numErrored;

                    continue;
                }

                { // write the shader bytecode to the temp file
                    FileByteWriter tempWriter(outputFilepath.Data());

                    if (!tempWriter.IsOpen())
                    {
                        Mutex::Guard guard(errorMessagesMutex);
                        outBundle->errorMessages.PushBack(HYP_FORMAT("Could not open file {} for writing!", outputFilepath));

                        ++numErrored;

                        continue;
                    }

                    tempWriter.Write(byteBuffer.Data(), byteBuffer.Size());
                    tempWriter.Close();
                }

                const String relativePath = FilePath(item.file).Basename();

                // for HLSL, we use entry point name based on stage
                shader->AddShaderModule(item.type, relativePath, byteBuffer.ToByteView());

                ++numCompiled;
            }

            for (const ShaderProperty& shaderProperty : perm.GetPropertySet())
            {
                // should be no longer permutable or value group by the time we get here (they should be "unwrapped")
                AssertDebug(!shaderProperty.IsPermutable() && !shaderProperty.IsValueGroup());

                const ShaderPropertyId propertyId = InternShaderProperty(shaderProperty);

                // Strip out the bundle's staticProperties from the individual shader properties.
                if (outBundle->staticProperties.Contains(propertyId))
                {
                    continue;
                }

                shader->properties.Add(propertyId);
            }
            shader->propertySetHashCode = perm.GetPropertySetHashCode();

            numCompiledPermutations += (numErrored == 0 && numCompiled > 0 ? 1 : 0);
            numErroredPermutations += (numErrored > 0 ? 1 : 0);

            if (numCompiled == 0)
            {
                HYP_LOG(ShaderCompiler, Warning, "No shader bytecode files were output for {}\n\tProperties: [{}]",
                    decl.name,
                    staticPropertiesString);
            }
            else if (numErrored == 0)
            {
                shader->inputGroup = ShaderInputGroup();
                descriptorUsageSetsMerged.BuildDescriptorTableDeclaration(shader->inputGroup);

#if HYP_ENABLE_SHADER_RELOAD
                shader->lastCompiledTimestamp = maxSourceFileLastModified;
#endif

                Mutex::Guard guard(compiledShadersMutex);

                AssertDebug(!usedNames.Contains(shader->GetName()));
                usedNames.Add(shader->GetName());

                auto existingIt = outBundle->compiledShaders.FindIf([name = shader->GetName()](const Handle<Shader>& existing)
                    {
                        if (existing->GetName() == name)
                        {
                            return true;
                        }

                        return false;
                    });

                if (existingIt != outBundle->compiledShaders.End())
                {
                    existingShadersToRemove.PushBack(std::move(*existingIt));
                    *existingIt = std::move(shader);
                }
                else
                {
                    outBundle->compiledShaders.PushBack(std::move(shader));
                }
            }
        },
        true);

    outBundle->MarkDirty();

    if (existingShadersToRemove.Any())
    {
        for (Handle<Shader>& shader : existingShadersToRemove)
        {
            // If we're using PrecompileShaders.exe, the pipeline caches will be null,
            // so make sure we don't dereference them
            if (!m_isPrecompilingShaders)
            {
                RI.graphicsPipelineCache->ExpirePipelinesForShader(shader);
                RI.computePipelineCache->ExpirePipelinesForShader(shader);
                RI.rayTracingPipelineCache->ExpirePipelinesForShader(shader);
            }

            GetEngineAssetRegistry()->RemoveAsset(shader);

            EnqueueDeletion(std::move(shader));
        }
    }

    if (outBundle->HasErrors())
    {
        HYP_LOG(ShaderCompiler, Error,
            "Shader compilation failed for shader {} with {} errored permutations!",
            decl.name, numErroredPermutations);

        for (const String& errorMessage : outBundle->errorMessages)
        {
            HYP_LOG(ShaderCompiler, Error, "\t{}", errorMessage);
        }

        return false;
    }

    if (outBundle->compiledShaders.Empty())
    {
        HYP_LOG(ShaderCompiler, Error,
            "No compiled shaders were produced for shader {}",
            decl.name);

        return false;
    }

    { // Save the shader property DB

        const FilePath shaderPropertyDbPath = GetCacheDirectory() / "ShaderProperties.bin";

        FileByteWriter shaderPropertyDbWriter { shaderPropertyDbPath };
        WriteShaderPropertyDictionary(shaderPropertyDbWriter);
        shaderPropertyDbWriter.Close();
    }

    // keep compiled shaders sorted.
    // partially this is to minimize changes causing excessive diffing in source control,
    // but also sorting by the amount of bits in the flag mask lets us find more "specific" variants earlier on
    // when selecting which variant to use.
    std::sort(
        outBundle->compiledShaders.Begin(),
        outBundle->compiledShaders.End(),
        [](const Handle<Shader>& a, const Handle<Shader>& b) -> bool
        {
            if (ByteUtil::BitCount(a->inputLayout.mask) < ByteUtil::BitCount(b->inputLayout.mask))
                return false;

            if (std::strcmp(a->GetName().LookupString(), b->GetName().LookupString()) < 0)
                return false;

            return true;
        });

    GetEngineAssetRegistry()->PutAssetsDeep(MakeStrongRef(outBundle));
    GetEngineAssetRegistry()->SaveDirtyAssets();

#ifdef HYP_SHADER_COMPILER_LOGGING
    if (numCompiledPermutations.Get(MemoryOrder::RELAXED) != 0)
    {
        HYP_LOG(ShaderCompiler, Info,
            "Compiled {} new variants for shader {} to: {}",
            numCompiledPermutations.Get(MemoryOrder::RELAXED), decl.name,
            finalOutputPath);
    }
#endif

    return true;
}

bool ShaderCompiler::RequestShader(
    Name name,
    ShaderPropertySet properties,
    VertexInputLayoutDesc inputLayout,
    Shader*& outShader)
{
    MergeGlobalShaderProperties(/* isPrecompilingShaders */ false, properties);

    Handle<ShaderBundle> bundle;

    if (!LoadBundle(name, ShaderRequest { properties, inputLayout }, bundle))
    {
        HYP_LOG(ShaderCompiler, Error, "Failed to attempt loading of shader bundle: {}", name);

        return false;
    }

    if (bundle->compiledShaders.Empty())
    {
        AssertDebug(false, "Loaded shader bundle has no compiled shaders! Corrupted file?");
        return false;
    }

    auto it = bundle->compiledShaders.FindIf(
        [&properties, &inputLayout](const Handle<Shader>& shader) -> bool
        {
            return SatisfiesRequested(properties, inputLayout, *shader, /* matchAllProperties */ true);
        });

    if (it == bundle->compiledShaders.End()
        && (!CanCompileShaders() && !m_isPrecompilingShaders))
    {
        // try again but this time only match the required properties, not all properties
        it = bundle->compiledShaders.FindIf(
            [&properties, &inputLayout](const Handle<Shader>& shader) -> bool
            {
                return SatisfiesRequested(properties, inputLayout, *shader, /* matchAllProperties */ false);
            });
    }

    if (it == bundle->compiledShaders.End())
    {
        HYP_LOG(ShaderCompiler, Error,
            "No match found for requested shader!\n"
            "Name: {}\n"
            "\tRequested properties: {}\n\tVertex Attributes: {}\n\n"
            "Found: {}",
            name, properties.GetDebugString(), InputLayoutToString(inputLayout),
            String::Join(bundle->compiledShaders, "\n", [](const Handle<Shader>& shader)
                {
                    return HYP_FORMAT("-----\n\tProperties: {}\n\tVertex Attributes: {}\n-----",
                        shader->properties.GetDebugString(), InputLayoutToString(shader->inputLayout));
                }));

        return false;
    }

    Assert((*it)->IsValid());

    outShader = it->Get();

#ifdef HYP_SHADER_COMPILER_LOGGING
    HYP_LOG(ShaderCompiler, Verbose,
        "Selected shader {} with properties: {}, attributes: {}",
        name,
        finalProperties.ToString(),
        finalProperties.GetRequiredVertexAttributes().ToString());
#endif

    return true;
}

bool ShaderCompiler::IsGraphicsShaderBundle(Name name) const
{
    if (!m_definitions || !m_definitions->IsValid())
    {
        return false;
    }

    for (const ShaderBundleDecl& decl : m_shaderBundleDecls)
    {
        if (decl.name == name)
        {
            return decl.sources.Contains(ShaderModuleType::Vertex);
        }
    }

    return false;
}

#if HYP_ENABLE_SHADER_RELOAD

bool ShaderCompiler::IsShaderBundleOutdated(Name name, const Time& lastCompiledTimestamp) const
{
    if (!CanCompileShaders())
    {
        return false;
    }

    const ShaderBundleDecl* foundDecl = nullptr;

    for (const ShaderBundleDecl& decl : m_shaderBundleDecls)
    {
        if (decl.name == name)
        {
            foundDecl = &decl;
            break;
        }
    }

    if (foundDecl == nullptr)
    {
        return false;
    }

    Time maxSourceFileLastModified = Time(0);

    for (const auto& sourceFile : foundDecl->sources)
    {
        maxSourceFileLastModified = MathUtil::Max(
            maxSourceFileLastModified,
            FilePath(sourceFile.second).LastModifiedTimestamp());
    }

    return maxSourceFileLastModified > lastCompiledTimestamp;
}

#endif // HYP_ENABLE_SHADER_RELOAD

#pragma endregion ShaderCompiler

} // namespace Hyperion

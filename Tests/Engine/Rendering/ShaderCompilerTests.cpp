/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/Shared.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/Vertex.hpp>

#include <Core/Core.hpp>
#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/TaskThread.hpp>
#include <Core/Threading/ThreadPool.hpp>

namespace Hyperion {
namespace tests {
namespace rendering {

namespace {

/// Platform the test binary is running on, used as the TARGET for compiles
ShaderCompileTargetPlatform GetHostPlatform()
{
#if HYP_WINDOWS
    return ShaderCompileTargetPlatform::Windows;
#elif HYP_MACOS
    return ShaderCompileTargetPlatform::Mac;
#elif HYP_LINUX
    return ShaderCompileTargetPlatform::Linux;
#else
    return ShaderCompileTargetPlatform::None;
#endif
}

/// Worker thread for the generic pool the tests start
class ShaderCompilerTestThread final : public TaskThread
{
public:
    ShaderCompilerTestThread(const ThreadId& id)
        : TaskThread(id, ThreadPriorityValue::NORMAL)
    {
    }

    virtual ~ShaderCompilerTestThread() override = default;
};

/// The test commandlet doesn't run the engine driver, so neither g_shaderCompiler nor the engine asset registry exist.
/// Set up both the same way PrecompileShaders does, then the tests own their compiler.
ShaderCompiler& GetShaderCompiler()
{
    static ShaderCompiler s_compiler;
    static const bool s_initialized = []()
    {
        if (!GetEngineAssetRegistry().IsValid())
        {
            Handle<AssetRegistry> engineRegistry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Engine,
                EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());

            engineRegistry->Initialize(nullptr);

            SetEngineAssetRegistry(engineRegistry);
        }

        // the engine driver starts the task system with its thread pools; the test commandlet doesn't, and
        // compiling a bundle with several permutations enqueues on the generic pool
        if (!TaskSystem::GetInstance().IsRunning())
        {
            TaskSystem::GetInstance().RegisterPool(
                TaskThreadPoolName::THREAD_POOL_GENERIC,
                MakeUnique<TaskThreadPool>(TypeWrapper<ShaderCompilerTestThread>(), "ShaderCompilerTests", 4));

            TaskSystem::GetInstance().Start();
        }

        // every platform and backend the compiler knows about, so the permutations requested below are all compilable
        const ShaderCompileParams params(
            ShaderCompileTargetPlatform::AllPlatforms | ShaderCompileTargetPlatform::Web,
            ShaderCompileTargetBackend::AllBackends | ShaderCompileTargetBackend::WebGPU);

        return s_compiler.Initialize(/* precompileShaders */ false, params);
    }();

    static_cast<void>(s_initialized);

    return s_compiler;
}

/// The TARGET property value the compiler matches permutations on
Name GetPlatformPropertyValue(ShaderCompileTargetPlatform platform)
{
    switch (platform)
    {
    case ShaderCompileTargetPlatform::Windows:
        return NAME("WINDOWS");
    case ShaderCompileTargetPlatform::Mac:
        return NAME("MAC");
    case ShaderCompileTargetPlatform::Linux:
        return NAME("LINUX");
    case ShaderCompileTargetPlatform::Web:
        return NAME("WEB");
    default:
        return Name::Invalid();
    }
}

/// The BACKEND property value the compiler matches permutations on
Name GetBackendPropertyValue(ShaderCompileTargetBackend backend)
{
    switch (backend)
    {
    case ShaderCompileTargetBackend::Vulkan:
        return NAME("VULKAN");
    case ShaderCompileTargetBackend::DX12:
        return NAME("DX12");
    case ShaderCompileTargetBackend::WebGPU:
        return NAME("WEBGPU");
    default:
        return Name::Invalid();
    }
}

/// Requests the compiled Fallback shader for one target, or nullptr if the compiler couldn't produce one
Shader* RequestCompiledFallback(ShaderCompiler& compiler, ShaderCompileTargetPlatform platform, ShaderCompileTargetBackend backend)
{
    ShaderPropertySet properties;
    properties.Add(InternShaderProperty(ShaderProperty(NAME("TARGET"), GetPlatformPropertyValue(platform))));
    properties.Add(InternShaderProperty(ShaderProperty(NAME("BACKEND"), GetBackendPropertyValue(backend))));

    Shader* shader = nullptr;

    if (!compiler.RequestShader(NAME("Fallback"), properties, VertexInputLayoutDesc(), shader))
    {
        return nullptr;
    }

    return shader;
}

/// Checks that the shader has a vertex and a pixel module
bool HasVertexAndPixelModules(const Shader& shader)
{
    bool hasVertex = false;
    bool hasPixel = false;

    for (ShaderModuleType type : shader.moduleTypes)
    {
        hasVertex |= type == ShaderModuleType::Vertex;
        hasPixel |= type == ShaderModuleType::Pixel;
    }

    return hasVertex && hasPixel;
}

/// True if the check passes for the compiled blob of every module of the shader
bool EveryModuleBlobPasses(Shader& shader, bool (*check)(ConstByteView))
{
    if (shader.moduleTypes.Empty())
    {
        return false;
    }

    // blob data is paged in by a read scope, and only resident while it is held
    auto readScope = shader.GetReadScope();

    for (ShaderModuleType moduleType : shader.moduleTypes)
    {
        if (!check(shader.GetModuleBlob(moduleType)))
        {
            return false;
        }
    }

    return true;
}

/// True if the blob starts with the SPIR-V magic number
bool IsSpirv(ConstByteView blob)
{
    return blob.Size() >= 4 && *reinterpret_cast<const uint32*>(blob.Data()) == 0x07230203;
}

/// True if the blob is a DXBC container
bool IsDxbc(ConstByteView blob)
{
    return blob.Size() >= 4
        && blob[0] == 'D' && blob[1] == 'X' && blob[2] == 'B' && blob[3] == 'C';
}

/// True if the blob contains a WGSL function declaration
bool IsWgsl(ConstByteView blob)
{
    for (size_t i = 0; i + 2 < blob.Size(); i++)
    {
        if (blob[i] == 'f' && blob[i + 1] == 'n' && blob[i + 2] == ' ')
        {
            return true;
        }
    }

    return false;
}

} // namespace

// Vulkan SPIR-V is compiled on every host, so the production Fallback shader must build here
HYP_ENGINE_TEST(ShaderCompiler, CompilesFallbackForHostVulkan)
{
    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

    HYP_TEST_CHECK(compiler.CompileVariantForTarget(
        NAME("Fallback"),
        ShaderPropertySet(),
        VertexInputLayoutDesc(),
        GetHostPlatform(),
        ShaderCompileTargetBackend::Vulkan));
}

// Compiling the same permutation twice must succeed both times (the second call is served from the compile cache)
HYP_ENGINE_TEST(ShaderCompiler, RepeatedCompileOfSameVariantSucceeds)
{
    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

    for (uint32 i = 0; i < 2; i++)
    {
        HYP_TEST_CHECK(compiler.CompileVariantForTarget(
            NAME("Fallback"),
            ShaderPropertySet(),
            VertexInputLayoutDesc(),
            GetHostPlatform(),
            ShaderCompileTargetBackend::Vulkan));
    }
}

// DX12 only builds on Windows, so this is a no-op on other hosts
HYP_ENGINE_TEST(ShaderCompiler, CompilesFallbackForHostDX12)
{
    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

#if HYP_WINDOWS
    HYP_TEST_CHECK(compiler.CompileVariantForTarget(
        NAME("Fallback"),
        ShaderPropertySet(),
        VertexInputLayoutDesc(),
        ShaderCompileTargetPlatform::Windows,
        ShaderCompileTargetBackend::DX12));
#endif
}

// Shaders that use the DECLARE_SRV / DECLARE_UAV / DECLARE_SAMPLER markers must go through the full descriptor pipeline
HYP_ENGINE_TEST(ShaderCompiler, CompilesDescriptorHeavyProductionShaders)
{
    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

    HYP_TEST_CHECK(compiler.CompileVariantForTarget(
        NAME("Tonemap"),
        ShaderPropertySet(),
        VertexInputLayoutDesc { VT_Simple },
        GetHostPlatform(),
        ShaderCompileTargetBackend::Vulkan));

    // DeferredDirect is a set of light-type permutations, so one has to be requested explicitly
    ShaderPropertySet lightProperties;
    lightProperties.Add(InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("DIRECTIONAL"))));

    HYP_TEST_CHECK(compiler.CompileVariantForTarget(
        NAME("DeferredDirect"),
        lightProperties,
        VertexInputLayoutDesc { VT_Simple },
        GetHostPlatform(),
        ShaderCompileTargetBackend::Vulkan));
}

// The Vulkan output for the host must be SPIR-V in every module, with both a vertex and a pixel stage
HYP_ENGINE_TEST(ShaderCompiler, VulkanOutputIsSpirvForEveryStage)
{
    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

    Shader* shader = RequestCompiledFallback(compiler, GetHostPlatform(), ShaderCompileTargetBackend::Vulkan);
    HYP_TEST_REQUIRE(shader != nullptr);

    HYP_TEST_CHECK(HasVertexAndPixelModules(*shader));
    HYP_TEST_CHECK(EveryModuleBlobPasses(*shader, &IsSpirv));
}

// The DX12 output must be a DXBC container for every stage, on Windows only
HYP_ENGINE_TEST(ShaderCompiler, Dx12OutputIsDxilContainerForEveryStage)
{
#if HYP_WINDOWS
    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

    Shader* shader = RequestCompiledFallback(compiler, ShaderCompileTargetPlatform::Windows, ShaderCompileTargetBackend::DX12);
    HYP_TEST_REQUIRE(shader != nullptr);

    HYP_TEST_CHECK(HasVertexAndPixelModules(*shader));
    HYP_TEST_CHECK(EveryModuleBlobPasses(*shader, &IsDxbc));
#endif
}

// WebGPU is opt-in; the SPIR-V output is converted to WGSL by tint, so the test only runs where tint is installed
// beside the engine (the same lookup the compiler does by default; a configured ShaderCompiler.TintPath isn't checked here)
HYP_ENGINE_TEST(ShaderCompiler, CompilesFallbackForWebGPU)
{
#ifdef HYP_WINDOWS
    const char* tintExecutableName = "tint.exe";
#else
    const char* tintExecutableName = "tint";
#endif

    const bool hasTint = (CoreApi::GetExecutablePath() / tintExecutableName).Exists()
        || (CoreApi::GetBaseDirectory() / "External/ThirdParty/Binaries/Windows/Release" / tintExecutableName).Exists();

    if (!hasTint)
    {
        return;
    }

    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

    HYP_TEST_CHECK(compiler.CompileVariantForTarget(
        NAME("Fallback"),
        ShaderPropertySet(),
        VertexInputLayoutDesc(),
        ShaderCompileTargetPlatform::Web,
        ShaderCompileTargetBackend::WebGPU));

    Shader* shader = RequestCompiledFallback(compiler, ShaderCompileTargetPlatform::Web, ShaderCompileTargetBackend::WebGPU);
    HYP_TEST_REQUIRE(shader != nullptr);

    HYP_TEST_CHECK(HasVertexAndPixelModules(*shader));
    HYP_TEST_CHECK(EveryModuleBlobPasses(*shader, &IsWgsl));
}

// A TARGET value that isn't a known platform must never produce a compile
HYP_ENGINE_TEST(ShaderCompiler, RejectsNonePlatform)
{
    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

    HYP_TEST_CHECK(!compiler.CompileVariantForTarget(
        NAME("Fallback"),
        ShaderPropertySet(),
        VertexInputLayoutDesc(),
        ShaderCompileTargetPlatform::None,
        ShaderCompileTargetBackend::Vulkan));
}

// A BACKEND value that isn't a known backend must never produce a compile
HYP_ENGINE_TEST(ShaderCompiler, RejectsNoneBackend)
{
    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

    HYP_TEST_CHECK(!compiler.CompileVariantForTarget(
        NAME("Fallback"),
        ShaderPropertySet(),
        VertexInputLayoutDesc(),
        GetHostPlatform(),
        ShaderCompileTargetBackend::None));
}

// A bundle name missing from Config/Shaders.hmf must fail cleanly rather than crash
HYP_ENGINE_TEST(ShaderCompiler, RejectsUnknownBundle)
{
    ShaderCompiler& compiler = GetShaderCompiler();
    HYP_TEST_REQUIRE(compiler.HasShaderBundle(NAME("Fallback")));

    HYP_TEST_CHECK(!compiler.CompileVariantForTarget(
        NAME("NoSuchShaderBundle"),
        ShaderPropertySet(),
        VertexInputLayoutDesc(),
        GetHostPlatform(),
        ShaderCompileTargetBackend::Vulkan));
}

// The DECLARE_* markers in DescriptorSets.inl register into the static descriptor table at startup
HYP_ENGINE_TEST(ShaderCompiler, GlobalDescriptorTableIsPopulated)
{
    HYP_TEST_CHECK(GetStaticDescriptorTableDeclaration().elements.Any());
}

} // namespace rendering
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS

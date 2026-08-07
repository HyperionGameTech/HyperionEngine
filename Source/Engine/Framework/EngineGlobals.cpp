/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Core/Core.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Utilities/Uuid.hpp>

#include <Core/Threading/SharedMutex.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <System/DirectoryInitializer.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>

#include <Asset/BlobStorage.hpp>

namespace Hyperion {

#ifndef HYP_SHIPPING
struct CookingContext;
#endif // !HYP_SHIPPING

struct CacheServerContext;

namespace EngineGlobals {

/// Editor build only
#ifdef HYP_EDITOR

ENGINE_API bool IsEditor()
{
    const CommandLineArguments& cliArgs = CoreApi::GetCommandLineArguments();

    return cliArgs["Editor"].ToBool();
}

HYP_EXPORT const FilePath& GetProjectsDirectory()
{
    // @TODO Use configuration value for this path. can be in Documents folder eg

    static DirectoryInitializer<HYP_STATIC_STRING("Projects"), /* RelativeToExecutablePath */ false> s_projectsDirectory;
    return s_projectsDirectory.path;
}

HYP_EXPORT const FilePath& GetDataDirectory()
{
    static DirectoryInitializer<HYP_STATIC_STRING("Data"), /* RelativeToExecutablePath */ false> s_dataDirectory;
    return s_dataDirectory.path;
}

#endif // HYP_EDITOR

#ifndef HYP_SHIPPING

ENGINE_API bool IsCooking()
{
    return IsGlobalContextActive<CookingContext>();
}

#endif // !HYP_SHIPPING

ENGINE_API bool IsCacheServer()
{
    return IsGlobalContextActive<CacheServerContext>();
}

// Directory for cached data (shader bundles, compiled scripts, etc.) Expected to be compiled into the asset registry in production builds
static AtomicVar<bool> s_cacheDirectoryInit = false;
static Mutex s_cacheDirectoryMutex;

HYP_EXPORT const FilePath& GetCacheDirectory()
{
    static const String& s_cfgCacheDirectory = CoreApi::GetCommandLineArguments()["CacheDir"].ToString();

    static const FilePath s_cacheDirectory =  (s_cfgCacheDirectory.Any()
        ? (s_cfgCacheDirectory.StartsWith(".")
            // Relative path - starts with . (eg "../Foo" or "./Foo")
            ? (CoreApi::GetExecutablePath() / s_cfgCacheDirectory)
            // Just use provided path.
            : s_cfgCacheDirectory)
        // Use fallback
        : (CoreApi::GetExecutablePath() / "Cache"));

    if (s_cacheDirectoryInit.Get(MemoryOrder::RELAXED))
    {
        return s_cacheDirectory;
    }

    Mutex::Guard guard(s_cacheDirectoryMutex);

    if (s_cacheDirectoryInit.Get(MemoryOrder::RELAXED))
    {
        return s_cacheDirectory;
    }

    if (s_cacheDirectory.Empty() || (!s_cacheDirectory.Exists() && !s_cacheDirectory.MkDir()))
    {
        HYP_LOG(Engine, Warning, "Failed to initialize cache storage directory {}!", s_cacheDirectory);
    }

    s_cacheDirectoryInit.Set(true, MemoryOrder::RELAXED);

    HYP_LOG(Engine, Info, "Initialized cache directory at {}", s_cacheDirectory);

    return s_cacheDirectory;
}

HYP_EXPORT const char* GetCacheServerAddress()
{
    static const String s_cacheServerAddress = CoreApi::GetCommandLineArguments()["CacheServer"].ToString();
    return s_cacheServerAddress.Data();
}

HYP_EXPORT const FilePath& GetConfigDirectory()
{
#ifndef HYP_SHIPPING
    static DirectoryInitializer<HYP_STATIC_STRING("Config"), /* RelativeToExecutablePath */ false> s_configDirectory;
    return s_configDirectory.path;
#else
    static DirectoryInitializer<HYP_STATIC_STRING("Config"), /* RelativeToExecutablePath */ true> s_configDirectory;
    return s_configDirectory.path;
#endif
}

HYP_EXPORT const FilePath& GetTempDirectory()
{
#if HYP_ANDROID
    // not used in Android build.
    static const FilePath s_emptyPath;
    return s_emptyPath;
#else  // !HYP_ANDROID
    static DirectoryInitializer<HYP_STATIC_STRING("Temp"), /* RelativeToExecutablePath */ true> s_tempDirectory;
    return s_tempDirectory.path;
#endif // HYP_ANDROID
}

HYP_EXPORT FilePath CreateTempDirectory()
{
    const FilePath& basePath = GetTempDirectory();

    if (basePath.Empty())
    {
        return FilePath();
    }

    if (!basePath.Exists() && !basePath.MkDir())
    {
        return FilePath();
    }

    for (uint32 attempt = 0; attempt < 16; ++attempt)
    {
        const String uuidString = UUID().ToString().ReplaceAll("-", "");
        const String randomSuffix = String(uuidString.Substr(0, 6));
        const FilePath tempPath = basePath / randomSuffix;

        if (tempPath.MkDir())
        {
            return tempPath;
        }
    }

    // Failed!
    return FilePath();
}

template <auto PackageName>
HYP_EXPORT const FilePath& GetContentDirectory()
{
#ifndef HYP_SHIPPING
    // Not shipping - Content at base dir of repo, subdir'd by package name.
    static DirectoryInitializer<HYP_STATIC_STRING("Content").template Concat<HYP_STATIC_STRING("/")>().template Concat<PackageName>(), /* RelativeToExecutablePath */ false> s_contentDir;
    return s_contentDir.path;
#else   // HYP_SHIPPING
    // Just use base Content directory at exe path.
    // Everything gets coalesced.
    static DirectoryInitializer<HYP_STATIC_STRING("Content"), /* RelativeToExecutablePath */ true> s_contentDir;
    return s_contentDir.path;
#endif  // !HYP_SHIPPING
}

template const FilePath& GetContentDirectory<HYP_STATIC_STRING("Editor")>();
template const FilePath& GetContentDirectory<HYP_STATIC_STRING("Engine")>();
template const FilePath& GetContentDirectory<HYP_STATIC_STRING("Game")>();

HYP_EXPORT bool IsShuttingDown()
{
    return g_engineDriver.IsValid()
        && g_engineDriver->IsShuttingDown();
}

BlobStorage g_blobStorage;

HYP_EXPORT BlobStorage* GetBlobStorage()
{
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, []()
                   {
                       if (!EngineGlobals::IsCooking())
                       {
                           g_blobStorage.Initialize();
                       }
                   });

    return &g_blobStorage;
}

} // namespace EngineGlobals
} // namespace Hyperion

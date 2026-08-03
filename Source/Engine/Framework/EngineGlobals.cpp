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

namespace Hyperion {

#ifndef HYP_SHIPPING
struct CookingContext;
#endif // !HYP_SHIPPING

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

// Directory for cached data (shader bundles, compiled scripts, etc.) Expected to be compiled into the asset registry in production builds
static AtomicVar<bool> s_cacheDirectoryInit = false;
static Mutex s_cacheDirectoryMutex;

HYP_EXPORT const FilePath& GetCacheDirectory()
{
    static const ConfigValue& s_cfgCacheDirectory = CoreApi::GetGlobalConfig().Get("App.Cache.BaseDirectory");

    static const FilePath s_cacheDirectory = CoreApi::GetExecutablePath() / s_cfgCacheDirectory.ToString().ToUtf8();

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

    return s_cacheDirectory;
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

HYP_EXPORT const FilePath& GetLibraryDirectory()
{
#ifndef HYP_SHIPPING
    static DirectoryInitializer<HYP_STATIC_STRING("Packages"), /* RelativeToExecutablePath */ false> s_resourceDirectory;
    return s_resourceDirectory.path;
#else   // HYP_SHIPPING
    static DirectoryInitializer<HYP_STATIC_STRING("Packages"), /* RelativeToExecutablePath */ true> s_resourceDirectory;

    if (!s_resourceDirectory.path.Exists())
    {
        HYP_LOG(Engine, Warning, "GetLibraryDirectory() called but Packages directory does not exist: {}",
                s_resourceDirectory.path.Data());
    }

    return s_resourceDirectory.path;
#endif  // !HYP_SHIPPING
}

HYP_EXPORT bool IsShuttingDown()
{
    return g_engineDriver.IsValid()
        && g_engineDriver->IsShuttingDown();
}

} // namespace EngineGlobals
} // namespace Hyperion

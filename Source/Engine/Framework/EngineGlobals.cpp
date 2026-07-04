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

#include <Asset/BlobStorage.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {
namespace EngineGlobals {

/// Editor build only
#if HYP_EDITOR

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

// Directory for cached data (shader bundles, compiled scripts, etc.) Expected to be compiled into the asset registry in production builds
static bool s_cacheDirectoryInit = false;
static SharedMutex s_cacheDirectoryMutex;

HYP_EXPORT const FilePath& GetCacheDirectory()
{
    static const ConfigValue& s_cfgCacheDirectory = CoreApi::GetGlobalConfig().Get("App.Cache.BaseDirectory");
    static const ConfigValue& s_cfgCachePageSize = CoreApi::GetGlobalConfig().Get("App.Cache.PageSize");

    static const FilePath s_cacheDirectory = CoreApi::GetExecutablePath() / s_cfgCacheDirectory.ToString().ToUtf8();

    TSharedLock sharedLock(s_cacheDirectoryMutex);

    if (s_cacheDirectoryInit)
        return s_cacheDirectory;

    sharedLock.Reset();

    TUniqueLock uniqueLock(s_cacheDirectoryMutex);

    if (s_cacheDirectoryInit)
        return s_cacheDirectory;

    if (!s_cfgCachePageSize.IsNumber() || s_cfgCachePageSize.AsNumber() < 1024 * 1024)
    {
        ConfigBase newConfigurationTable;
        newConfigurationTable.Set("App.Cache.PageSize", ConfigValue(BlobStorage::DefaultPageSize));

        CoreApi::UpdateGlobalConfig(newConfigurationTable);
    }

    if (s_cacheDirectory.Empty() || (!s_cacheDirectory.Exists() && !s_cacheDirectory.MkDir()))
    {
        HYP_FAIL("Failed to initialize cache storage directory {}!", s_cacheDirectory);
    }

    s_cacheDirectoryInit = true;

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
#ifdef HYP_EDITOR
    static DirectoryInitializer<HYP_STATIC_STRING("Packages"), /* RelativeToExecutablePath */ false> s_resourceDirectory;
    return s_resourceDirectory.path;
#else  // !HYP_EDITOR
    static DirectoryInitializer<HYP_STATIC_STRING("Packages"), /* RelativeToExecutablePath */ true> s_resourceDirectory;

    if (!s_resourceDirectory.path.Exists())
    {
        HYP_LOG(Engine, Warning, "GetLibraryDirectory() called but Packages directory does not exist: {}",
                s_resourceDirectory.path.Data());
    }

    return s_resourceDirectory.path;
#endif // HYP_EDITOR
}

} // namespace EngineGlobals
} // namespace Hyperion

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
#include <Framework/CVarManager.hpp>

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


bool g_isCommandlet = false;
bool g_isServer = false;
bool g_isHeadless = false;

ENGINE_API bool IsCacheServer()
{
    return IsGlobalContextActive<CacheServerContext>();
}

ENGINE_API bool IsCommandlet()
{
    static std::once_flag s_init;
    std::call_once(
        s_init,
        []
        {
            g_isCommandlet = CoreApi::GetCommandLineArguments()["exec"].ToBool();
        });

    return g_isCommandlet;
}

ENGINE_API bool IsServer()
{
    static std::once_flag s_init;
    std::call_once(
        s_init,
        []
        {
            g_isServer = CoreApi::GetCommandLineArguments()["server"].ToBool();
        });

    return g_isServer;
}

ENGINE_API bool IsHeadless()
{
    static std::once_flag s_init;
    std::call_once(
        s_init,
        []
        {
            g_isHeadless = IsCommandlet() || IsServer() || CoreApi::GetCommandLineArguments()["Headless"].ToBool();
        });

    return g_isHeadless;
}

static FilePath s_cacheDirectory;

// Directory for cached data (shader bundles, compiled scripts, etc.) Expected to be compiled into the asset registry in production builds
HYP_EXPORT const FilePath& GetCacheDirectory()
{
    static std::once_flag s_onceFlag;
    std::call_once(
        s_onceFlag,
        []
        {
            const String cfgValue = CoreApi::GetCommandLineArguments()["CacheDir"].ToString();

            if (cfgValue.Any())
            {
                s_cacheDirectory = (cfgValue.StartsWith(".")
                    // Relative path - starts with . (eg "../Foo" or "./Foo")
                    ? (CoreApi::GetExecutablePath() / cfgValue)
                    // Just use provided path.
                    : cfgValue);
            }
            else
            {
                s_cacheDirectory = CoreApi::GetExecutablePath() / "Cache";
            }

            if (s_cacheDirectory.Empty() || (!s_cacheDirectory.Exists() && !s_cacheDirectory.MkDir()))
            {
                HYP_LOG(Engine, Warning, "Failed to initialize cache storage directory {}!", s_cacheDirectory);
            }

            HYP_LOG(Engine, Info, "Initialized cache directory at {}", s_cacheDirectory);
        });

    return s_cacheDirectory;
}

template <auto PackageName>
HYP_EXPORT const FilePath& GetContentDirectory()
{
    static FilePath s_contentDirectory;
    static std::once_flag s_onceFlag;

    std::call_once(
        s_onceFlag,
        []
        {
            const String cfgValue = CoreApi::GetCommandLineArguments()["ContentDir"].ToString();

            if (cfgValue.Any())
            {
                s_contentDirectory = (cfgValue.StartsWith(".")
                    // Relative path - starts with . (eg "../Foo" or "./Foo")
                    ? (CoreApi::GetExecutablePath() / cfgValue)
                    // Just use provided path.
                    : cfgValue);
            }
            else
            {
#if !defined(HYP_SHIPPING) && !defined(HYP_ANDROID) && !defined(HYP_IOS)
                if constexpr (!Memory::StrEqual(PackageName.data, "Game", 4))
                {
                    // <base>/Content/Engine
                    // <base>/Content/Editor
                    s_contentDirectory = CoreApi::GetBaseDirectory() / "Content" / String(PackageName.data);
                }
                else
#endif // !SHIPPING && !ANDROID && !IOS
                {
                    // <exe>/Content
                    s_contentDirectory = CoreApi::GetExecutablePath() / "Content";
                }
            }

            if (s_contentDirectory.Empty() || (!s_contentDirectory.Exists() && !s_contentDirectory.MkDir()))
            {
                HYP_LOG(Engine, Warning, "Failed to initialize content storage directory {}!", s_contentDirectory);
            }

            HYP_LOG(Engine, Info, "Initialized content directory at {} for package {}", s_contentDirectory, PackageName.data);
        });

    return s_contentDirectory;
}

template const FilePath& GetContentDirectory<HYP_STATIC_STRING("Editor")>();
template const FilePath& GetContentDirectory<HYP_STATIC_STRING("Engine")>();
template const FilePath& GetContentDirectory<HYP_STATIC_STRING("Game")>();

HYP_EXPORT const char* GetCacheServerAddress()
{
    static const String s_cacheServerAddress = CoreApi::GetCommandLineArguments()["CacheServer"].ToString();
    return s_cacheServerAddress.Data();
}

HYP_EXPORT const char* GetHostAddress()
{
    static const String s_hostAddress = CoreApi::GetCommandLineArguments()["host"].ToString();
    return s_hostAddress.Data();
}

HYP_EXPORT uint16 GetGameServerPort()
{
    static CVar<uint16> s_cvGameServerPort("Net.GameServerPort", 9192);
    return s_cvGameServerPort.Get();
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

HYP_EXPORT bool IsShuttingDown()
{
    return g_engineDriver.IsValid()
        && g_engineDriver->IsShuttingDown();
}

BlobStorage g_blobStorage;

HYP_EXPORT BlobStorage* GetBlobStorage()
{
    return &g_blobStorage;
}

} // namespace EngineGlobals
} // namespace Hyperion

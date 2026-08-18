/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class AppContextBase;
class EngineDriver;
class AssetManager;
class AudioManager;
class DeletionQueue;
class ShaderManager;
class MaterialCache;
class RenderInterface;
class ShaderCompiler;
class ShaderManager;
class EditorState;
class StreamingManager;
class SimThread;
class RenderThread;
class MainThread;
class VisThread;
class EngineStats;
class InputManager;
class Game;
class GameServer;
class BlobStorage;
struct GameState;

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

#if HYP_VULKAN
class VulkanRenderInterface;
#elif HYP_DX12
class DX12RenderInterface;
#endif

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE

// Globals for internal usage within the Hyperion library

ENGINE_API extern Handle<EngineDriver> g_engineDriver;
ENGINE_API extern Handle<AssetManager> g_assetManager;
ENGINE_API extern Handle<AudioManager> g_audioManager;
ENGINE_API extern Handle<AppContextBase> g_appContext;
ENGINE_API extern Handle<StreamingManager> g_streamingManager;
ENGINE_API extern Handle<EngineStats> g_engineStats;
ENGINE_API extern MaterialCache* g_materialCache;
ENGINE_API extern ShaderCompiler* g_shaderCompiler;
ENGINE_API extern ShaderManager* g_shaderManager;
ENGINE_API extern GameServer* g_gameServer;

#ifdef HYP_EDITOR
extern Handle<EditorState> g_editorState;
#endif // HYP_EDITOR

namespace EngineGlobals {

#ifdef HYP_EDITOR
ENGINE_API bool IsEditor();
#else  // !HYP_EDITOR
static constexpr NoOpFunction<bool> IsEditor;
#endif // HYP_EDITOR

#ifndef HYP_SHIPPING
ENGINE_API bool IsCooking();
#else   // HYP_SHIPPING
static constexpr NoOpFunction<bool> IsCooking;
#endif  // !HYP_SHIPPING

ENGINE_API bool IsCacheServer();
ENGINE_API bool IsCommandlet();
ENGINE_API bool IsServer();
ENGINE_API bool IsHeadless();

#ifdef HYP_EDITOR
ENGINE_API const FilePath& GetProjectsDirectory();
ENGINE_API const FilePath& GetDataDirectory();
#endif // HYP_EDITOR

template <auto PackageName>
ENGINE_API const FilePath& GetContentDirectory();

extern template ENGINE_API const FilePath& GetContentDirectory<HYP_STATIC_STRING("Editor")>();
extern template ENGINE_API const FilePath& GetContentDirectory<HYP_STATIC_STRING("Engine")>();
extern template ENGINE_API const FilePath& GetContentDirectory<HYP_STATIC_STRING("Game")>();

ENGINE_API const FilePath& GetCacheDirectory();
ENGINE_API const char* GetCacheServerAddress();
ENGINE_API const FilePath& GetTempDirectory();
ENGINE_API const FilePath& GetConfigDirectory();
ENGINE_API HYP_NODISCARD FilePath CreateTempDirectory();

ENGINE_API bool IsShuttingDown();

ENGINE_API BlobStorage* GetBlobStorage();

} // namespace EngineGlobals

extern MainThread* g_mainThreadInstance;
extern SimThread* g_simThreadInstance;
extern RenderThread* g_renderThreadInstance;
extern VisThread* g_visThreadInstance;

extern Game* g_gameInstance;

#if HYP_VULKAN
extern VulkanRenderInterface RI;
#elif HYP_DX12
extern DX12RenderInterface RI;
#endif // HYP_VULKAN || HYP_DX12

#endif // HYPERION_ENGINE

} // namespace Hyperion

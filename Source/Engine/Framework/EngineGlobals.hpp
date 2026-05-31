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
class MaterialInstanceCache;
class RenderInterface;
class ShaderCompiler;
class EditorState;
class StreamingManager;
class SimThread;
class RenderThread;
class MainThread;
class VisThread;
class EngineStats;
class InputManager;
class Game;
struct GameState;

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
ENGINE_API extern MaterialInstanceCache* g_materialInstanceCache;
ENGINE_API extern ShaderCompiler* g_shaderCompiler;

#if HYP_EDITOR
extern Handle<EditorState> g_editorState;
#endif

extern MainThread* g_mainThreadInstance;
extern SimThread* g_simThreadInstance;
extern RenderThread* g_renderThreadInstance;
extern VisThread* g_visThreadInstance;

extern Game* g_gameInstance;

#if HYP_VULKAN
extern VulkanRenderInterface RI;
#elif HYP_DX12
extern DX12RenderInterface RI;
#endif

#endif

} // namespace Hyperion

/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/Types.hpp>

namespace hyperion {

class AppContextBase;
class EngineDriver;
class AssetManager;
class SafeDeleter;
class ShaderManager;
class MaterialCache;
class RenderInterface;
class IRenderBackend;
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

#ifdef HYP_VULKAN
class VulkanRenderBackend;
#endif

#if defined(HYPERION_ENGINE) && HYPERION_ENGINE

// Globals for internal usage within the Hyperion library

extern Handle<EngineDriver> g_engineDriver;
extern Handle<AssetManager> g_assetManager;
extern Handle<EditorState> g_editorState;
extern Handle<AppContextBase> g_appContext;
extern Handle<StreamingManager> g_streamingManager;
extern Handle<EngineStats> g_engineStats;
extern ShaderManager* g_shaderManager;
extern MaterialCache* g_materialCache;
extern SafeDeleter* g_safeDeleter;
extern RenderInterface* g_renderInterface;
extern ShaderCompiler* g_shaderCompiler;

extern MainThread* g_mainThreadInstance;
extern SimThread* g_simThreadInstance;
extern RenderThread* g_renderThreadInstance;
extern VisThread* g_visThreadInstance;

extern Handle<Game> g_gameInstance;

#ifdef HYP_VULKAN
extern VulkanRenderBackend* g_renderBackend;
#endif

#endif

} // namespace hyperion

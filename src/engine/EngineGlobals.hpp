/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/Types.hpp>

namespace hyperion {

namespace sys {
class AppContextBase;
} // namespace sys

using sys::AppContextBase;

class EngineDriver;
class AssetManager;
class SafeDeleter;
class ShaderManager;
class MaterialCache;
class RenderGlobalState;
class IRenderBackend;
class ShaderCompiler;
class EditorState;
class StreamingManager;

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
extern ShaderManager* g_shaderManager;
extern MaterialCache* g_materialSystem;
extern SafeDeleter* g_safeDeleter;
extern RenderGlobalState* g_renderGlobalState;
extern ShaderCompiler* g_shaderCompiler;

#ifdef HYP_VULKAN
extern VulkanRenderBackend* g_renderBackend;
#endif

#endif

} // namespace hyperion

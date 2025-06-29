/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENGINE_GLOBALS_HPP
#define HYPERION_ENGINE_GLOBALS_HPP

#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class AssetManager;
class SafeDeleter;
class ShaderManager;
class MaterialCache;
class RenderGlobalState;
class IRenderBackend;

extern Handle<Engine> g_engine;
extern Handle<AssetManager> g_asset_manager;
extern ShaderManager* g_shader_manager;
extern MaterialCache* g_material_system;
extern SafeDeleter* g_safe_deleter;
extern IRenderBackend* g_render_backend;
extern RenderGlobalState* g_render_global_state;

} // namespace hyperion

#endif

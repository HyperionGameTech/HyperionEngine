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

namespace renderer {
class IRenderingAPI;
} // namespace renderer

using renderer::IRenderingAPI;

extern Handle<Engine> g_engine;
extern Handle<AssetManager> g_asset_manager;
extern ShaderManager* g_shader_manager;
extern MaterialCache* g_material_system;
extern SafeDeleter* g_safe_deleter;
extern IRenderingAPI* g_rendering_api;

} // namespace hyperion

#endif

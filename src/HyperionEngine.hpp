/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>
#include <core/Defines.hpp>
#include <core/config/Config.hpp>

namespace hyperion {

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

namespace cli {

class CommandLineArguments;

} // namespace cli

using cli::CommandLineArguments;

namespace memory {
class Pool;
} // namespace memory

using memory::Pool;

HYP_API extern Pool* g_objectPool;                  // Pool for object allocations - not thread safe per se, but used with proper locking in HypObjectPool
HYP_API extern Pool* g_renderPool;                  // Pool for rendering allocations (render thread only)
HYP_API extern Pool* g_framePools[NumMultiBuffers]; // Pools for per-frame allocations, on either game or render thread for their given frame index.
HYP_API extern Pool* g_scenePool;                   // Pool for scene-related allocations (thread safe)
HYP_API extern Pool* g_taskPool;                    // Pool for task system allocations (thread safe)

HYP_API const FilePath& GetResourceDirectory();

HYP_API bool InitializeEngine(int argc, char** argv);
HYP_API void DestroyEngine();

} // namespace hyperion

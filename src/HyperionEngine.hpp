/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <system/AppContext.hpp>

#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <Engine.hpp>

namespace hyperion {

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

HYP_API void InitializeAppContext(const RC<AppContext> &app_context, Game *game);

HYP_API void InitializeEngine(const FilePath &base_path);
HYP_API void DestroyEngine();

} // namespace hyperion
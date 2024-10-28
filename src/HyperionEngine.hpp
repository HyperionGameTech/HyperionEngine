/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/system/AppContext.hpp>
#include <core/Defines.hpp>
#include <Engine.hpp>

namespace hyperion {

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

HYP_API void InitializeAppContext(const RC<AppContext> &app_context);

HYP_API void InitializeEngine(const FilePath &base_path);
HYP_API void DestroyEngine();

} // namespace hyperion
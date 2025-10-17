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

HYP_API const FilePath& GetResourceDirectory();

HYP_API bool InitializeEngine(int argc, char** argv);
HYP_API void DestroyEngine();

} // namespace hyperion

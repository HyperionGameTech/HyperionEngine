/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/Result.hpp>

namespace Hyperion {

static constexpr const char* BaseNamespace = "Hyperion";
namespace CodeGen {

Optional<String> ExtractCXXClassName(const String& line);
Array<String> ExtractCXXBaseClasses(const String& line);

/// \brief Extracts the C++ namespace for the given source code. (built up)
/// Parses 'namespace foo::bar' as well as nested 'namespace foo { namespace bar { ... } }' constructs.
Array<String> ExtractCXXNamespacePath(const String& source);

bool IsCXXClassDecl(const String& line);
bool IsCXXStructDecl(const String& line);
bool IsCXXEnumDecl(const String& line);
bool IsCXXEnumClassDecl(const String& line);

String GetDateTimeString();

String GetGeneratedFilePreamble(const String& srcPath);

Result ReplaceFileIfDifferent(FilePath& tempFilePath, const FilePath& targetFilePath);

} // namespace CodeGen
} // namespace Hyperion

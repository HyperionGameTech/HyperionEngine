/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/Optional.hpp>
#include <Core/Utilities/Result.hpp>

namespace Hyperion {

static constexpr const char* BaseNamespace = "Hyperion";

struct ClassAttributeValue; 

namespace CodeGen {

Optional<String> ExtractCXXClassName(const String& line);
Array<String> ExtractCXXBaseClasses(const String& line);

/// \brief Extracts the C++ namespace for the given source code. (built up)
/// Parses 'namespace foo::bar' as well as nested 'namespace foo { namespace bar { ... } }' constructs.
Array<String> ExtractCXXNamespacePath(const String& source);

String BuildNamespaceString(Span<const String> namespaceParts);

bool IsCXXClassDecl(const String& line);
bool IsCXXStructDecl(const String& line);
bool IsCXXEnumDecl(const String& line);
bool IsCXXEnumClassDecl(const String& line);

String GetDateTimeString();

String GetGeneratedFilePreamble(const String& srcPath);

Result ReplaceFileIfDifferent(FilePath& tempFilePath, const FilePath& targetFilePath);

bool CheckAttrCSV(const ClassAttributeValue& attrValue, const String& expected);

} // namespace CodeGen
} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Utilities/Pair.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/Span.hpp>

#include <Core/Defines.hpp>

#include <Core/DataProcessing/Shared/Utils.hpp>

namespace Hyperion {
class ByteReader;
class Class;
} // namespace Hyperion

namespace Hyperion::DataProcessing {

template <class TErrorType>
class ErrorList;

class CompilerError;

} // namespace Hyperion::DataProcessing

namespace Hyperion::DataProcessing::HMF {

using ErrorList = ::Hyperion::DataProcessing::ErrorList<CompilerError>;

using ParseResult = TResult<BoxedValue>;

using ResolveAssetPathFn = bool (*)(const String& path, const TypeInfo& targetType, BoxedValue& out);

CORE_API extern ResolveAssetPathFn g_resolveAssetPath;

struct SchemaSectionEntry
{
    Name key;
    Array<Pair<Name, BoxedValue>> values;
};

/*! \brief An object whose class wasn't registered when it was parsed into a generic (BoxedValue) slot, eg a component defined by a
 *  script that hasn't loaded yet. Kept as HMF source text so it can be written back unchanged, or parsed again once the class exists. */
struct UnresolvedObject
{
    Name className;
    String source;
};

struct UnresolvedValue
{
    String source;
};

using ParseSchemaSectionFn = bool (*)(BoxedValue& owner, Array<SchemaSectionEntry>&& entries);

CORE_API void SetParseSchemaSectionFn(const ANSIStringView& sectionName, ParseSchemaSectionFn fn);
CORE_API ParseSchemaSectionFn GetParseSchemaSectionFn(const ANSIStringView& sectionName);

/*! \brief Redirects let HMF written before a class or member was renamed still load. They are only consulted when a name
 *  isn't found, so the normal path pays nothing. Register them where the renamed type lives, eg from a static initializer. */
CORE_API void AddClassRedirect(const ANSIStringView& oldClassName, const ANSIStringView& newClassName);

/*! \brief Redirects a member of \p className (and its subclasses) from \p oldMemberName to \p newMemberName */
CORE_API void AddMemberRedirect(const ANSIStringView& className, const ANSIStringView& oldMemberName, const ANSIStringView& newMemberName);

/*! \brief Returns the current name of a renamed class, or an invalid StringHash if \p className has no redirect */
CORE_API StringHash GetClassRedirect(StringHash className);

/*! \brief Returns the current name of a renamed member of \p cls or one of its parents, or an invalid StringHash if there's no redirect */
CORE_API StringHash GetMemberRedirect(const Class* cls, StringHash memberName);

////////////////////

CORE_API ParseResult Parse(const FilePath& filePath, ByteReader& reader, ErrorList* outErrors = nullptr, BoxedValue* target = nullptr);
CORE_API ParseResult Parse(const FilePath& filePath, const String& source, ErrorList* outErrors = nullptr, BoxedValue* target = nullptr);
CORE_API ParseResult Parse(ByteReader& reader, ErrorList* outErrors = nullptr, BoxedValue* target = nullptr);
CORE_API ParseResult Parse(const String& source, ErrorList* outErrors = nullptr, BoxedValue* target = nullptr);

struct ParseOptions
{
    // When not empty, only these fields of the top-level object are read; the others are skipped without being constructed
    Span<const StringHash> topLevelFields;
};

CORE_API ParseResult Parse(const FilePath& filePath, ByteReader& reader, const ParseOptions& options, ErrorList* outErrors = nullptr);

/*! \brief Reads a single value of the given type, e.g the source of an UnresolvedValue */
CORE_API ParseResult ParseValue(const String& source, const TypeInfo& typeInfo, ErrorList* outErrors = nullptr);

} // namespace Hyperion::DataProcessing::HMF

namespace Hyperion {
namespace HMF = DataProcessing::HMF;
} // namespace Hyperion

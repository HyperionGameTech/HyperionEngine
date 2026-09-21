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

#include <Core/Defines.hpp>


namespace Hyperion {
class ByteReader;
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
    String className;
    String source;
};

using ParseSchemaSectionFn = bool (*)(BoxedValue& owner, Array<SchemaSectionEntry>&& entries);

CORE_API void SetParseSchemaSectionFn(const ANSIStringView& sectionName, ParseSchemaSectionFn fn);
CORE_API ParseSchemaSectionFn GetParseSchemaSectionFn(const ANSIStringView& sectionName);

////////////////////

CORE_API ParseResult Parse(const FilePath& filePath, ByteReader& reader, ErrorList* outErrors = nullptr, BoxedValue* target = nullptr);
CORE_API ParseResult Parse(const FilePath& filePath, const String& source, ErrorList* outErrors = nullptr, BoxedValue* target = nullptr);
CORE_API ParseResult Parse(ByteReader& reader, ErrorList* outErrors = nullptr, BoxedValue* target = nullptr);
CORE_API ParseResult Parse(const String& source, ErrorList* outErrors = nullptr, BoxedValue* target = nullptr);

} // namespace Hyperion::DataProcessing::HMF

namespace Hyperion {
namespace HMF = DataProcessing::HMF;
} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <cstdlib>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/DataProcessing/Shared/SourceStream.hpp>
#include <Core/DataProcessing/Shared/TokenStream.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>
#include <Core/DataProcessing/Shared/Lexer.hpp>

#include <Core/DataProcessing/HMF/Parser/Parser.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/Class.hpp>

namespace Hyperion::DataProcessing::HMF {

ResolveAssetPathFn g_resolveAssetPath = nullptr;

namespace /* Helpers */ {

const FilePath g_inMemoryFilePath = FilePath("<memory-buffer>");

struct Redirects
{
    Mutex mutex;

    // old class name hash -> current class name hash
    Map<StringHash, StringHash> classes;
    // owning class name -> (old member name -> current member name)
    Map<StringHash, Map<StringHash, StringHash>> members;
};

Redirects& GetRedirects()
{
    static Redirects s_redirects;
    return s_redirects;
}

ParseResult RunParse(
    const FilePath& filePath,
    ByteReader& reader,
    ErrorList* outErrors,
    BoxedValue* target = nullptr,
    const ParseOptions* options = nullptr)
{
    ErrorList errorList;

    const ByteBuffer source = reader.Read();
    MemoryByteReader sourceReader(source.ToByteView());

    SourceStream sourceStream { &sourceReader, filePath };
    TokenStream tokenStream { TokenStreamInfo(filePath) };

    Lexer lexer(sourceStream, &tokenStream, &errorList);
    lexer.Analyze();

    Parser parser(&tokenStream, &errorList, target, options);
    
    BoxedValue resultValue;

    if (target != nullptr)
    {
        parser.Parse();

        resultValue = *target;
    }
    else
    {
        // grab value from parse result
        parser.Parse(resultValue, /* moveResult */ true);
    }

    if (outErrors)
    {
        outErrors->Concatenate(errorList);
    }

    if (!resultValue.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Parse failed");
    }

    return resultValue;
}

Map<ANSIString, ParseSchemaSectionFn>& GetParseSchemaSectionFns()
{
    static Map<ANSIString, ParseSchemaSectionFn> s_parseSchemaSectionFns;
    return s_parseSchemaSectionFns;
}

Mutex& GetParseSchemaSectionFnsMutex()
{
    static Mutex s_parseSchemaSectionFnsMutex;
    return s_parseSchemaSectionFnsMutex;
}

} // namespace anonymous

ParseSchemaSectionFn GetParseSchemaSectionFn(const ANSIStringView& sectionName)
{
    Mutex::Guard guard(GetParseSchemaSectionFnsMutex());

    Map<ANSIString, ParseSchemaSectionFn>& parseSchemaSectionFns = GetParseSchemaSectionFns();

    const auto it = parseSchemaSectionFns.Find(sectionName);

    if (it == parseSchemaSectionFns.End())
    {
        return nullptr;
    }

    return it->second;
}

void SetParseSchemaSectionFn(
    const ANSIStringView& sectionName,
    ParseSchemaSectionFn fn)
{
    Mutex::Guard guard(GetParseSchemaSectionFnsMutex());

    GetParseSchemaSectionFns()[sectionName] = fn;
}

void AddClassRedirect(
    const ANSIStringView& oldClassName,
    const ANSIStringView& newClassName)
{
    Redirects& redirects = GetRedirects();
    Mutex::Guard guard(redirects.mutex);

    redirects.classes[StringHash(oldClassName)] = StringHash(newClassName);
}

void AddMemberRedirect(
    const ANSIStringView& className,
    const ANSIStringView& oldMemberName,
    const ANSIStringView& newMemberName)
{
    Redirects& redirects = GetRedirects();
    Mutex::Guard guard(redirects.mutex);

    redirects.members[StringHash(className)][StringHash(oldMemberName)] = StringHash(newMemberName);
}

StringHash GetClassRedirect(StringHash className)
{
    Redirects& redirects = GetRedirects();
    Mutex::Guard guard(redirects.mutex);

    const auto it = redirects.classes.Find(className);

    if (it == redirects.classes.End())
    {
        return StringHash();
    }

    return it->second;
}

StringHash GetMemberRedirect(const Class* cls, StringHash memberName)
{
    Redirects& redirects = GetRedirects();
    Mutex::Guard guard(redirects.mutex);

    for (; cls != nullptr; cls = cls->GetParent())
    {
        const auto classIt = redirects.members.Find(StringHash(cls->GetName()));

        if (classIt == redirects.members.End())
        {
            continue;
        }

        const auto memberIt = classIt->second.Find(memberName);

        if (memberIt != classIt->second.End())
        {
            return memberIt->second;
        }
    }

    return StringHash();
}

ParseResult Parse(
    const FilePath& filePath,
    ByteReader& reader,
    ErrorList* outErrors,
    BoxedValue* target)
{
    return RunParse(filePath, reader, outErrors, target);
}

ParseResult Parse(
    const FilePath& filePath,
    const String& source,
    ErrorList* outErrors,
    BoxedValue* target)
{
    MemoryByteReader reader(ConstByteView(reinterpret_cast<const ubyte*>(source.Data()), source.Size()));

    return RunParse(filePath, reader, outErrors, target);
}

ParseResult Parse(
    ByteReader& reader,
    ErrorList* outErrors,
    BoxedValue* target)
{
    return RunParse(g_inMemoryFilePath, reader, outErrors, target);
}

ParseResult Parse(
    const String& source,
    ErrorList* outErrors,
    BoxedValue* target)
{
    return Parse(g_inMemoryFilePath, source, outErrors, target);
}

ParseResult Parse(
    const FilePath& filePath,
    ByteReader& reader,
    const ParseOptions& options,
    ErrorList* outErrors)
{
    return RunParse(filePath, reader, outErrors, nullptr, &options);
}

ParseResult ParseValue(
    const String& source,
    const TypeInfo& typeInfo,
    ErrorList* outErrors)
{
    ErrorList errorList;

    MemoryByteReader sourceReader(ConstByteView(reinterpret_cast<const ubyte*>(source.Data()), source.Size()));

    SourceStream sourceStream { &sourceReader, g_inMemoryFilePath };
    TokenStream tokenStream { TokenStreamInfo(g_inMemoryFilePath) };

    Lexer lexer(sourceStream, &tokenStream, &errorList);
    lexer.Analyze();

    Parser parser(&tokenStream, &errorList);

    BoxedValue resultValue;
    const bool parsed = parser.ParseSingleValue(typeInfo, resultValue);

    if (outErrors)
    {
        outErrors->Concatenate(errorList);
    }

    if (!parsed || !resultValue.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Failed to read \"{}\" as {}", source, typeInfo.name);
    }

    return resultValue;
}

} // namespace Hyperion::DataProcessing::HMF

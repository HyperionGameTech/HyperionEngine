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

namespace Hyperion::DataProcessing::HMF {

ResolveAssetPathFn g_resolveAssetPath = nullptr;

static const FilePath s_inMemoryFilePath = FilePath("<memory-buffer>");

namespace {

ParseResult RunParse(
    const FilePath& filePath,
    ByteReader& reader,
    ErrorList* outErrors,
    BoxedValue* target = nullptr)
{
    ErrorList errorList;

    SourceStream sourceStream { &reader, filePath };
    TokenStream tokenStream { TokenStreamInfo(filePath) };

    Lexer lexer(sourceStream, &tokenStream, &errorList);
    lexer.Analyze();

    Parser parser(&tokenStream, &errorList, target);
    
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

} // namespace anonymous

ParseResult Parse(const FilePath& filePath, ByteReader& reader, ErrorList* outErrors, BoxedValue* target)
{
    return RunParse(filePath, reader, outErrors, target);
}

ParseResult Parse(const FilePath& filePath, const String& source, ErrorList* outErrors, BoxedValue* target)
{
    MemoryByteReader reader(ConstByteView(reinterpret_cast<const ubyte*>(source.Data()), source.Size()));

    return RunParse(filePath, reader, outErrors, target);
}

ParseResult Parse(ByteReader& reader, ErrorList* outErrors, BoxedValue* target)
{
    return RunParse(s_inMemoryFilePath, reader, outErrors, target);
}

ParseResult Parse(const String& source, ErrorList* outErrors, BoxedValue* target)
{
    return Parse(s_inMemoryFilePath, source, outErrors, target);
}

} // namespace Hyperion::DataProcessing::HMF

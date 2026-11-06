#pragma once

#include <Core/DataProcessing/Shared/SourceStream.hpp>
#include <Core/DataProcessing/Shared/TokenStream.hpp>
#include <Core/DataProcessing/Shared/SourceLocation.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>
#include <Core/DataProcessing/Shared/CompilerError.hpp>

#include <Core/Unicode.hpp>

namespace Hyperion::DataProcessing {

class Lexer
{
public:
    // lexes the source set on tokenStream (TokenStream::SetSource)
    Lexer(
        TokenStream* tokenStream,
        ErrorList<CompilerError>* errorList);
    Lexer(const Lexer& other);

    void Analyze();
    Token NextToken();
    utf::Char32 ReadEscapeCode();
    Token ReadStringLiteral(TokenClass tokenClass = TK_STRING);
    Token ReadAssetPathLiteral();
    Token ReadNumberLiteral();
    Token ReadHexNumberLiteral();
    Token ReadLineComment();
    Token ReadBlockComment();
    Token ReadDocumentation();
    Token ReadIdentifier();

private:
    SourceStream m_sourceStream;
    TokenStream* m_tokenStream;
    ErrorList<CompilerError>* m_errorList;
    SourceLocation m_sourceLocation;

    bool HasNext();
    bool SkipWhitespace();

    Token ReadPunctuation(TokenClass tokenClass);
    UTF8StringView Slice(size_t start, size_t end) const;
};

} // namespace Hyperion::DataProcessing

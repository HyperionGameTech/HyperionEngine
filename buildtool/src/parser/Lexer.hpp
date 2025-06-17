/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_LEXER_HPP
#define HYPERION_BUILDTOOL_LEXER_HPP

#include <parser/SourceStream.hpp>
#include <parser/TokenStream.hpp>
#include <parser/SourceLocation.hpp>
#include <parser/CompilationUnit.hpp>
#include <parser/Operator.hpp>

#include <util/UTF8.hpp>

namespace hyperion {
namespace buildtool {

class Lexer
{
public:
    Lexer(
        const SourceStream& source_stream,
        TokenStream* token_stream,
        CompilationUnit* compilation_unit);
    Lexer(const Lexer& other);

    HYP_FORCE_INLINE SourceStream& GetSourceStream()
    {
        return m_source_stream;
    }

    HYP_FORCE_INLINE const SourceStream& GetSourceStream() const
    {
        return m_source_stream;
    }

    HYP_FORCE_INLINE TokenStream* GetTokenStream() const
    {
        return m_token_stream;
    }

    /** Forms the given TokenStream from the given SourceStream */
    void Analyze();
    /** Reads the next token and returns it */
    Token NextToken();
    /** Reads two characters that make up an escape code and returns actual value */
    utf::u32char ReadEscapeCode();
    /** Reads a string literal and returns the token */
    Token ReadStringLiteral();
    /** Reads a number literal and returns the token */
    Token ReadNumberLiteral();
    /** Reads a hex number literal and returns the token */
    Token ReadHexNumberLiteral();
    /** Reads a single-line comment */
    Token ReadLineComment();
    /** Reads a multi-line block comment */
    Token ReadBlockComment();
    /** Reads an important comment (documentation block) */
    Token ReadDocumentation();
    /** Reads an operator and returns the token */
    Token ReadOperator();
    /** Reads the name, and returns the either identifier or keyword token */
    Token ReadIdentifier();

private:
    SourceStream m_source_stream;
    TokenStream* m_token_stream;
    CompilationUnit* m_compilation_unit;
    SourceLocation m_source_location;

    /** Adds an end-of-file error if at the end, returns true if not */
    bool HasNext();
    /** Reads until there is no more whitespace.
        Returns true if a newline character was encountered.
    */
    bool SkipWhitespace();
};

} // namespace buildtool
} // namespace hyperion

#endif

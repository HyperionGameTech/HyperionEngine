/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/json/parser/SourceStream.hpp>
#include <Core/json/parser/TokenStream.hpp>
#include <Core/json/parser/SourceLocation.hpp>
#include <Core/json/parser/CompilationUnit.hpp>

#include <Core/Unicode.hpp>

namespace Hyperion::JSON {

class Lexer
{
public:
    Lexer(
        const SourceStream& sourceStream,
        TokenStream* tokenStream,
        CompilationUnit* compilationUnit);
    Lexer(const Lexer& other);

    /** Forms the given TokenStream from the given SourceStream */
    void Analyze();
    /** Reads the next token and returns it */
    Token NextToken();
    /** Reads two characters that make up an escape code and returns actual value */
    utf::Char32 ReadEscapeCode();
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
    /** Reads the name, and returns the either identifier or keyword token */
    Token ReadIdentifier();

private:
    SourceStream m_sourceStream;
    TokenStream* m_tokenStream;
    CompilationUnit* m_compilationUnit;
    SourceLocation m_sourceLocation;

    /** Adds an end-of-file error if at the end, returns true if not */
    bool HasNext();
    /** Reads until there is no more whitespace.
        Returns true if a newline character was encountered.
    */
    bool SkipWhitespace();
};

} // namespace Hyperion::JSON

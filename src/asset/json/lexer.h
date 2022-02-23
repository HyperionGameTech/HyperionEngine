#ifndef HYPERION_JSON_LEXER_H
#define HYPERION_JSON_LEXER_H

#include "source_stream.h"
#include "token_stream.h"
#include "source_location.h"
#include "state.h"
#include "../../util/utf8.h"

namespace hyperion {
namespace json {
class Lexer {
public:
    Lexer(const SourceStream &source_stream,
        TokenStream *token_stream,
        State *state);
    Lexer(const Lexer &other);

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
    /** Read a directive returns the token */
    Token ReadDirective();
    /** Reads the name, and returns the either identifier or keyword token */
    Token ReadIdentifier();

private:
    SourceStream m_source_stream;
    TokenStream *m_token_stream;
    State *m_state;
    SourceLocation m_source_location;

    /** Adds an end-of-file error if at the end, returns true if not */
    bool HasNext();
    /** Reads until there is no more whitespace.
        Returns true if a newline character was encountered.
    */
    bool SkipWhitespace();
};

} // namespace json
} // namespace hyperion

#endif
#ifndef HYPERION_JSON_PARSER_H
#define HYPERION_JSON_PARSER_H

#include "source_stream.h"
#include "token_stream.h"
#include "source_location.h"
#include "state.h"
#include "json.h"

#include <string>

namespace hyperion {
namespace json {
class Parser {
public:
    Parser(TokenStream *token_stream,
        State *state);
    Parser(const Parser &other);

    /** Forms the given TokenStream from the given SourceStream */
    JSONValue Parse();

private:
    TokenStream *m_token_stream;
    State *m_state;

    Token Match(TokenClass token_class, bool read = false);
    Token MatchAhead(TokenClass token_class, int n);
    Token MatchOperator(const ::std::string &op, bool read = false);
    Token Expect(TokenClass token_class, bool read = false);
    bool ExpectEndOfStmt();
    SourceLocation CurrentLocation() const;
    void SkipStatementTerminators();

    JSONValue ParseExpression();
    JSONValue ParseTerm();
    JSONString ParseStringLiteral();
    JSONNumber ParseIntegerLiteral();
    JSONNumber ParseFloatLiteral();
    JSONObject ParseObject();
    JSONArray ParseArray();
};

} // namespace json
} // namespace hyperion

#endif
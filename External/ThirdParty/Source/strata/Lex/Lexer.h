#pragma once

#include "strata/Core/Diagnostics.h"
#include "strata/Core/SourceLocation.h"
#include "strata/Lex/Token.h"

#include <stddef.h>

typedef struct {
    const char* m_source;
    size_t m_sourceLen;
    DiagnosticEngine* m_diag;
    size_t m_pos;
    bool m_hasPeek;
    Token m_peeked;
    uint16_t m_fileId;
} Lexer;

void LexerInit(Lexer* lex, const char* source, size_t sourceLen, DiagnosticEngine* diag, uint16_t fileId);

Token LexerNextToken(Lexer* lex);
Token LexerPeekToken(Lexer* lex);

size_t LexerPosition(const Lexer* lex);
bool LexerAtEnd(const Lexer* lex);
Str LexerSourceText(const Lexer* lex);

static inline char LexerPeek(const Lexer* lex, size_t ahead)
{
    size_t i = lex->m_pos + ahead;
    return (i < lex->m_sourceLen) ? lex->m_source[i] : '\0';
}

#pragma once

#include "strata/AST/AST.h"
#include "strata/Core/Diagnostics.h"
#include "strata/Core/Util.h"
#include "strata/Lex/Lexer.h"
#include "strata/Lex/Token.h"

typedef struct {
    Lexer* m_lex;
    DiagnosticEngine* m_diag;
    char* m_moduleName;
    Arena* m_arena;
    Token m_cur;
    const char* m_returnType;
} Parser;

void ParserInit(Parser* p, Lexer* lex, DiagnosticEngine* diag, Arena* arena, const char* moduleName);
Module* ParserParseModule(Parser* p);

Str ParserIdentText(const Parser* p, Token t);
bool ParserConsume(Parser* p, TokKind k);
Token ParserExpect(Parser* p, TokKind k, const char* what);
bool ParserTryParseType(Parser* p, TypeName* out);

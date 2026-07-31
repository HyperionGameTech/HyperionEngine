#pragma once

#include "strata/Core/SourceLocation.h"
#include "strata/Core/Util.h"

#include <stdint.h>

typedef enum {
    TokEof,
    TokUnknown,

    TokIdent,
    TokIntLit,
    TokFloatLit,
    TokBoolLit,

    TokKwVoid,
    TokKwBool,
    TokKwInt,
    TokKwUint,
    TokKwFloat,
    TokKwDouble,
    TokKwString,
    TokKwRef,
    TokKwConst,
    TokKwStatic,
    TokKwExtern,
    TokKwReturn,
    TokKwIf,
    TokKwElse,
    TokKwWhile,
    TokKwFor,
    TokKwBreak,
    TokKwContinue,
    TokKwTrue,
    TokKwFalse,
    TokKwStruct,
    TokKwHandle,
    TokKwImport,
    TokKwNamespace,

    TokLParen,
    TokRParen,
    TokLBrace,
    TokRBrace,
    TokLBracket,
    TokRBracket,
    TokComma,
    TokSemicolon,
    TokColon,
    TokDot,
    TokArrow,

    TokAssign,
    TokPlus,
    TokMinus,
    TokStar,
    TokSlash,
    TokPercent,
    TokAmp,
    TokPipe,
    TokCaret,
    TokTilde,
    TokBang,
    TokLt,
    TokGt,
    TokLtEq,
    TokGtEq,
    TokEqEq,
    TokNotEq,
    TokAmpAmp,
    TokPipePipe,
    TokShl,
    TokShr,
    TokPlusEq,
    TokMinusEq,
    TokStarEq,
    TokSlashEq,
    TokPercentEq,
    TokInc,
    TokDec,
} TokKind;

const char* TokSpelling(TokKind kind);
const char* TokName(TokKind kind);
TokKind ClassifyKeyword(Str ident);

typedef struct {
    TokKind kind;
    SourceRange range;
} Token;

static inline bool TokenIs(const Token* t, TokKind k)
{
    return t->kind == k;
}

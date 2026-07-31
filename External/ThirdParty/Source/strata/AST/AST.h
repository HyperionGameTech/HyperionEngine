#pragma once

#include "strata/Core/SourceLocation.h"
#include "strata/Core/Util.h"

#include <stdint.h>
#include <string.h>

typedef enum {
    NodeModule,
    NodeImport,
    NodeStruct,
    NodeHandle,
    NodeFunction,
    NodeParam,
    NodeBlock,
    NodeReturn,
    NodeIf,
    NodeWhile,
    NodeFor,
    NodeVarDecl,
    NodeExprStmt,
    NodeBreak,
    NodeContinue,
    NodeIntLiteral,
    NodeFloatLiteral,
    NodeBoolLiteral,
    NodeIdent,
    NodeUnary,
    NodeBinary,
    NodeAssign,
    NodeCall,
    NodeMember,
    NodeStructInit,
    NodeIncDec,
    NodeCast,
    NodeGlobal,
} NodeKind;

typedef struct {
    NodeKind kind;
    SourceRange range;
} Node;

#define AST_NEW(arena, type) ((type*)memset(arena_alloc((arena), sizeof(type)), 0, sizeof(type)))
#define AsNode(type, node) ((type*)(node))

typedef struct {
    char* name;
    SourceRange range;
    bool isConst;
} TypeName;

static inline bool TypeNameValid(const TypeName* t)
{
    return t->name != NULL && t->name[0] != '\0';
}

typedef enum {
    ModNone,
    ModRef,
} ParamMod;

static inline bool ByRef(ParamMod m)
{
    return m != ModNone;
}

static inline const char* ParamModSpelling(ParamMod m)
{
    switch (m)
    {
    case ModRef:
        return "ref";
    case ModNone:
        return "";
    }
    return "";
}

typedef struct {
    Node base;
    ParamMod mod;
    TypeName type;
    char* name;
} ParamDecl;

typedef struct {
    TypeName type;
    char* name;
} FieldDecl;

typedef struct {
    Node base;
    char* name;
    Vec fields;
} StructDecl;

typedef struct {
    Node base;
    char* name;
} HandleDecl;

typedef struct {
    Node base;
    TypeName returnType;
    char* name;
    Vec params;
    Node* body;
    bool isExtern;
    char* mangledName;
} FunctionDecl;

typedef struct {
    Node base;
    char* name;
    Vec structs;
    Vec handles;
    Vec functions;
    Vec globals;
    Vec imports;
} Module;

typedef struct {
    Node base;
    char* importPath;
    SourceRange pathRange;
} ImportDecl;

typedef struct {
    Node base;
    Vec statements;
} Block;

typedef struct {
    Node base;
    Node* value;
} ReturnStmt;

typedef struct {
    Node base;
    Node* condition;
    Node* thenBranch;
    Node* elseBranch;
} IfStmt;

typedef struct {
    Node base;
    Node* condition;
    Node* body;
} WhileStmt;

typedef struct {
    Node base;
    Node* init;
    Node* condition;
    Node* update;
    Node* body;
} ForStmt;

typedef struct {
    Node base;
    TypeName type;
    char* name;
    Node* init;
} VarDeclStmt;

typedef struct {
    Node base;
    Node* expr;
} ExprStmt;

typedef struct {
    Node base;
} BreakStmt;

typedef struct {
    Node base;
} ContinueStmt;

typedef enum {
    UnNeg,
    UnPos,
    UnNot,
    UnBitNot,
} UnaryOp;

typedef struct {
    Node base;
    UnaryOp op;
    Node* operand;
} UnaryExpr;

typedef enum {
    BinAdd,
    BinSub,
    BinMul,
    BinDiv,
    BinMod,
    BinBitAnd,
    BinBitOr,
    BinBitXor,
    BinShl,
    BinShr,
    BinEqEq,
    BinNotEq,
    BinLt,
    BinLtEq,
    BinGt,
    BinGtEq,
    BinLogicAnd,
    BinLogicOr,
} BinaryOp;

typedef struct {
    Node base;
    BinaryOp op;
    Node* lhs;
    Node* rhs;
} BinaryExpr;

typedef enum {
    AssignSet,
    AssignAdd,
    AssignSub,
    AssignMul,
    AssignDiv,
    AssignMod,
} AssignOp;

typedef struct {
    Node base;
    AssignOp op;
    Node* target;
    Node* value;
} AssignExpr;

typedef struct {
    Node base;
    uint64_t value;
    bool isUnsigned;
} IntLiteral;

typedef struct {
    Node base;
    double value;
} FloatLiteral;

typedef struct {
    Node base;
    bool value;
} BoolLiteral;

typedef struct {
    Node base;
    char* name;
} IdentExpr;

typedef struct {
    Node base;
    char* callee;
    const FunctionDecl* resolvedDecl;
    Vec args;
} CallExpr;

typedef struct {
    Node base;
    Node* base_node;
    char* member;
} MemberExpr;

typedef struct {
    char* name;
    Node* value;
} StructInitField;

typedef struct {
    Node base;
    char* typeName;
    Vec fields;
} StructInitExpr;

typedef struct {
    Node base;
    bool isDec;
    bool isPrefix;
    Node* operand;
} IncDecExpr;

typedef struct {
    Node base;
    TypeName type;
    Node* operand;
} CastExpr;

typedef struct {
    Node base;
    TypeName type;
    char* name;
    Node* init;
} GlobalDecl;

#pragma once

#include <script/compiler/type-system/SymbolType.hpp>

#include <memory>

namespace hyperion {

class SymbolType;
class IdentifierTable;

struct BuiltinTypes
{
    static const SymbolType* s_primitiveType;
    static const SymbolType* s_errorType;
    static const SymbolType* s_objectType;
    static const SymbolType* s_functionBaseType; // non-generic function type
    static const SymbolType* s_arrayBaseType;    // non-generic array type
    static const SymbolType* s_varArgsBaseType;
    static const SymbolType* s_mapBaseType; // non-generic map type
    static const SymbolType* s_anyType;
    static const SymbolType* s_classType;
    static const SymbolType* s_placeholderType;
    static const SymbolType* s_voidType;
    static const SymbolType* s_int8Type;
    static const SymbolType* s_int16Type;
    static const SymbolType* s_int32Type;
    static const SymbolType* s_int64Type;
    static const SymbolType* s_uint8Type;
    static const SymbolType* s_uint16Type;
    static const SymbolType* s_uint32Type;
    static const SymbolType* s_uint64Type;
    static const SymbolType* s_floatType;
    static const SymbolType* s_doubleType;
    static const SymbolType* s_boolType;
    static const SymbolType* s_stringType;
    static const SymbolType* s_varArgsType;
    static const SymbolType* s_functionType;
    static const SymbolType* s_nullType;
    static const SymbolType* s_arrayType;
    static const SymbolType* s_mapType;

    static void Initialize();
    static void AddToSymbolTable(IdentifierTable& table);
};

} // namespace hyperion

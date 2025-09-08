#pragma once

#include <script/compiler/type-system/SymbolType.hpp>

#include <memory>

namespace hyperion {

class SymbolType;
using SymbolTypeRef = RC<SymbolType>;

class SymbolTypeTrait;

class IdentifierTable;

struct BuiltinTypes
{
    static const SymbolTypeRef s_primitiveType;
    static const SymbolTypeRef s_errorType;
    static const SymbolTypeRef s_varArgsBaseType;
    static const SymbolTypeRef s_varArgsType;
    static const SymbolTypeRef s_objectType;
    static const SymbolTypeRef s_enumType;
    static const SymbolTypeRef s_anyType;
    static const SymbolTypeRef s_classType;
    static const SymbolTypeRef s_placeholderType;
    static const SymbolTypeRef s_voidType;
    static const SymbolTypeRef s_intType;
    static const SymbolTypeRef s_unsignedIntType;
    static const SymbolTypeRef s_floatType;
    static const SymbolTypeRef s_boolType;
    static const SymbolTypeRef s_stringType;
    static const SymbolTypeRef s_functionBaseType; // non-generic function type
    static const SymbolTypeRef s_functionType;
    static const SymbolTypeRef s_nullType;
    static const SymbolTypeRef s_arrayBaseType; // non-generic array type
    static const SymbolTypeRef s_arrayType;
    static const SymbolTypeRef s_mapBaseType; // non-generic map type
    static const SymbolTypeRef s_mapType;

    static void AddToSymbolTable(IdentifierTable& table);
};

} // namespace hyperion

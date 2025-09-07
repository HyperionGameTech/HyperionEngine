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
    static const SymbolTypeRef g_primitiveType;
    static const SymbolTypeRef g_errorType;
    static const SymbolTypeRef g_varArgsBaseType;
    static const SymbolTypeRef g_varArgsType;
    static const SymbolTypeRef g_objectType;
    static const SymbolTypeRef g_enumType;
    static const SymbolTypeRef g_anyType;
    static const SymbolTypeRef g_classType;
    static const SymbolTypeRef g_placeholderType;
    static const SymbolTypeRef g_voidType;
    static const SymbolTypeRef g_intType;
    static const SymbolTypeRef g_unsignedIntType;
    static const SymbolTypeRef g_floatType;
    static const SymbolTypeRef g_boolType;
    static const SymbolTypeRef g_stringType;
    static const SymbolTypeRef g_functionBaseType; // non-generic function type
    static const SymbolTypeRef g_functionType;
    static const SymbolTypeRef g_nullType;
    static const SymbolTypeRef g_arrayBaseType; // non-generic array type
    static const SymbolTypeRef g_arrayType;
    static const SymbolTypeRef g_mapBaseType; // non-generic map type
    static const SymbolTypeRef g_mapType;

    static void AddToSymbolTable(IdentifierTable& table);
};

} // namespace hyperion

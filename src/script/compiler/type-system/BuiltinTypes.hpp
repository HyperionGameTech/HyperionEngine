#pragma once

#include <script/compiler/type-system/SymbolType.hpp>

namespace hyperion {

class SymbolType;
class CompilationUnit;

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

    /*! \brief Initialize builtin types in the given global compilation unit. The
     *   global compilation unit is used for shared library types and must have a lifetime that exists
     *   for the entire duration of the program. (Or as long as HypScript is used.)
     */
    static void Initialize(CompilationUnit* globalCompilationUnit);
    static void RegisterTypes(CompilationUnit* compilationUnit);
};

} // namespace hyperion

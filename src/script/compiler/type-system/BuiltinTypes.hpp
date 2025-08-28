#pragma once

#include <script/compiler/type-system/SymbolType.hpp>

#include <memory>

namespace hyperion::compiler {

class SymbolType;
using SymbolTypeRef = RC<SymbolType>;

class SymbolTypeTrait;

struct BuiltinTypeTraits
{
    static const SymbolTypeTrait variadic;
};

struct BuiltinTypes
{
    static const SymbolTypeRef PRIMITIVE_TYPE;
    static const SymbolTypeRef UNDEFINED;
    static const SymbolTypeRef OBJECT;
    static const SymbolTypeRef CLASS_TYPE;
    static const SymbolTypeRef ENUM_TYPE;
    static const SymbolTypeRef ANY;
    static const SymbolTypeRef PLACEHOLDER;
    static const SymbolTypeRef VOID_TYPE;
    static const SymbolTypeRef INT;
    static const SymbolTypeRef UNSIGNED_INT;
    static const SymbolTypeRef FLOAT;
    static const SymbolTypeRef BOOLEAN;
    static const SymbolTypeRef STRING;
    static const SymbolTypeRef FUNCTION;
    static const SymbolTypeRef NULL_TYPE;
    static const SymbolTypeRef MODULE_INFO;
    static const SymbolTypeRef ARRAY;
    static const SymbolTypeRef GENERIC_VARIABLE_TYPE;
};

} // namespace hyperion::compiler

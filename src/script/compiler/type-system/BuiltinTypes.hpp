#ifndef BUILTIN_TYPES_HPP
#define BUILTIN_TYPES_HPP

#include <memory>

namespace hyperion::compiler {

class SymbolType;
using SymbolTypePtr_t = std::shared_ptr<SymbolType>;

struct BuiltinTypes
{
    static const SymbolTypePtr_t PRIMITIVE_TYPE;
    static const SymbolTypePtr_t UNDEFINED;
    static const SymbolTypePtr_t OBJECT;
    static const SymbolTypePtr_t CLASS_TYPE;
    static const SymbolTypePtr_t ANY;
    static const SymbolTypePtr_t PLACEHOLDER;
    static const SymbolTypePtr_t VOID_TYPE;
    static const SymbolTypePtr_t INT;
    static const SymbolTypePtr_t UNSIGNED_INT;
    static const SymbolTypePtr_t FLOAT;
    static const SymbolTypePtr_t BOOLEAN;
    static const SymbolTypePtr_t STRING;
    static const SymbolTypePtr_t FUNCTION;
    static const SymbolTypePtr_t ARRAY;
    static const SymbolTypePtr_t VAR_ARGS;
    static const SymbolTypePtr_t NULL_TYPE;
    static const SymbolTypePtr_t MODULE_INFO;
    static const SymbolTypePtr_t GENERATOR;
    static const SymbolTypePtr_t CLOSURE_TYPE;
    static const SymbolTypePtr_t GENERIC_VARIABLE_TYPE;
};

} // namespace hyperion::compiler

#endif

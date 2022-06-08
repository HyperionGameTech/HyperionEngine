#ifndef BUILTIN_TYPES_HPP
#define BUILTIN_TYPES_HPP

#include <memory>

namespace hyperion {
namespace compiler {

class SymbolType;
using SymbolTypePtr_t = std::shared_ptr<SymbolType>;

struct BuiltinTypes {
    static const SymbolTypePtr_t UNDEFINED;
    static const SymbolTypePtr_t OBJECT;
    static const SymbolTypePtr_t ANY;
    static const SymbolTypePtr_t INT;
    static const SymbolTypePtr_t FLOAT;
    static const SymbolTypePtr_t NUMBER;
    static const SymbolTypePtr_t BOOLEAN;
    static const SymbolTypePtr_t STRING;
    static const SymbolTypePtr_t FUNCTION;
    static const SymbolTypePtr_t ARRAY;
    static const SymbolTypePtr_t TUPLE;
    static const SymbolTypePtr_t VAR_ARGS;
    static const SymbolTypePtr_t NULL_TYPE;
    static const SymbolTypePtr_t EVENT;
    static const SymbolTypePtr_t EVENT_IMPL;
    static const SymbolTypePtr_t EVENT_ARRAY;
    static const SymbolTypePtr_t MODULE_INFO;
    static const SymbolTypePtr_t GENERATOR;
    static const SymbolTypePtr_t BOXED_TYPE;
    static const SymbolTypePtr_t MAYBE;
    static const SymbolTypePtr_t CONST_TYPE;
    static const SymbolTypePtr_t BLOCK_TYPE;
    static const SymbolTypePtr_t CLOSURE_TYPE;
};

} // namespace compiler
} // namespace hyperion

#endif

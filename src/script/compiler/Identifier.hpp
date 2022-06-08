#ifndef IDENTIFIER_HPP
#define IDENTIFIER_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <memory>

namespace hyperion {
namespace compiler {

enum IdentifierFlags {
   FLAG_CONST = 1,
   FLAG_ALIAS = 2,
   FLAG_MIXIN = 4,
   FLAG_DECLARED_IN_FUNCTION = 8,
};

class Identifier {
public:
    Identifier(const std::string &name, int index, int flags);
    Identifier(const Identifier &other);

    inline const std::string &GetName() const { return m_name; }
    inline int GetIndex() const { return m_index; }
    
    inline int GetStackLocation() const { return m_stack_location; }
    inline void SetStackLocation(int stack_location) { m_stack_location = stack_location; }

    inline void IncUseCount() const { m_usecount++; }
    inline void DecUseCount() const { m_usecount--; }
    inline int GetUseCount() const { return m_usecount; }

    inline int GetFlags() const { return m_flags; }
    inline int &GetFlags() { return m_flags; }
    inline void SetFlags(int flags) { m_flags = flags; }

    inline std::shared_ptr<AstExpression> GetCurrentValue() const { return m_current_value; }
    inline void SetCurrentValue(const std::shared_ptr<AstExpression> &expr) { m_current_value = expr; }
    /*inline const ObjectType &GetObjectType() const { return m_object_type; }
    inline void SetObjectType(const ObjectType &object_type) { m_object_type = object_type; }
    inline bool TypeCompatible(const ObjectType &other_type) const
        { return ObjectType::TypeCompatible(m_object_type, other_type); }*/

    inline const SymbolTypePtr_t &GetSymbolType() const { return m_symbol_type; }
    inline void SetSymbolType(const SymbolTypePtr_t &symbol_type) { m_symbol_type = symbol_type; }

private:
    std::string m_name;
    int m_index;
    int m_stack_location;
    mutable int m_usecount;
    int m_flags;
    std::shared_ptr<AstExpression> m_current_value;
    SymbolTypePtr_t m_symbol_type;
};

} // namespace compiler
} // namespace hyperion

#endif

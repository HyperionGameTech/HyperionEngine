#ifndef IDENTIFIER_HPP
#define IDENTIFIER_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

enum IdentifierFlags {
   FLAG_CONST                = 0b00000001,
   FLAG_ALIAS                = 0b00000010,
   FLAG_MIXIN                = 0b00000100,
   FLAG_GENERIC              = 0b00001000,
   FLAG_DECLARED_IN_FUNCTION = 0b00010000,
   FLAG_PLACEHOLDER          = 0b00100000
};

class Identifier {
public:
    Identifier(const std::string &name, int index, int flags, Identifier *aliasee=nullptr);
    Identifier(const Identifier &other);

    inline const std::string &GetName() const { return m_name; }
    inline int GetIndex() const { return Unalias()->m_index; }
    
    inline int GetStackLocation() const { return Unalias()->m_stack_location; }
    inline void SetStackLocation(int stack_location) { Unalias()->m_stack_location = stack_location; }

    inline void IncUseCount() const { Unalias()->m_usecount++; }
    inline void DecUseCount() const { Unalias()->m_usecount--; }
    inline int GetUseCount() const { return Unalias()->m_usecount; }

    inline int GetFlags() const { return m_flags; }
    inline int &GetFlags() { return m_flags; }
    inline void SetFlags(int flags) { m_flags = flags; }

    inline bool IsReassigned() const { return m_is_reassigned; }
    inline void SetIsReassigned(bool is_reassigned) { m_is_reassigned = is_reassigned; }

    inline std::shared_ptr<AstExpression> GetCurrentValue() const { return Unalias()->m_current_value; }
    inline void SetCurrentValue(const std::shared_ptr<AstExpression> &expr) { Unalias()->m_current_value = expr; }
    inline const SymbolTypePtr_t &GetSymbolType() const { return Unalias()->m_symbol_type; }
    inline void SetSymbolType(const SymbolTypePtr_t &symbol_type) { Unalias()->m_symbol_type = symbol_type; }

    inline const std::vector<GenericInstanceTypeInfo::Arg> &GetTemplateParams() const
        { return Unalias()->m_template_params; }

    inline void SetTemplateParams(const std::vector<GenericInstanceTypeInfo::Arg> &template_params)
        { Unalias()->m_template_params = template_params; }

    inline Identifier *Unalias() {  return (m_aliasee != nullptr) ? m_aliasee : this; }
    inline const Identifier *Unalias() const {  return (m_aliasee != nullptr) ? m_aliasee : this; }

private:
    std::string m_name;
    int m_index;
    int m_stack_location;
    mutable int m_usecount;
    int m_flags;
    Identifier *m_aliasee;
    std::shared_ptr<AstExpression> m_current_value;
    SymbolTypePtr_t m_symbol_type;
    bool m_is_reassigned;

    std::vector<GenericInstanceTypeInfo::Arg> m_template_params;
};

} // namespace hyperion::compiler

#endif

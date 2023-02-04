#ifndef IDENTIFIER_HPP
#define IDENTIFIER_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <Types.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

using IdentifierFlagBits = UInt;

enum IdentifierFlags : IdentifierFlagBits
{
   FLAG_CONST                = 0x1,
   FLAG_ALIAS                = 0x2,
   FLAG_MIXIN                = 0x4,
   FLAG_GENERIC              = 0x8,
   FLAG_DECLARED_IN_FUNCTION = 0x10,
   FLAG_PLACEHOLDER          = 0x20,
   FLAG_ACCESS_PRIVATE       = 0x40,
   FLAG_ACCESS_PUBLIC        = 0x80,
   FLAG_ACCESS_PROTECTED     = 0x100,
   FLAG_ARGUMENT             = 0x200,
   FLAG_REF                  = 0x400,
   FLAG_ENUM                 = 0x800,
   FLAG_MEMBER               = 0x1000
};

class Identifier
{
public:
    Identifier(const std::string &name, int index, IdentifierFlagBits flags, Identifier *aliasee = nullptr);
    Identifier(const Identifier &other);

    const std::string &GetName() const { return m_name; }
    int GetIndex() const { return Unalias()->m_index; }
    
    int GetStackLocation() const { return Unalias()->m_stack_location; }
    void SetStackLocation(int stack_location) { Unalias()->m_stack_location = stack_location; }

    void IncUseCount() const { Unalias()->m_usecount++; }
    void DecUseCount() const { Unalias()->m_usecount--; }
    int GetUseCount() const { return Unalias()->m_usecount; }

    IdentifierFlagBits GetFlags() const { return m_flags; }
    IdentifierFlagBits &GetFlags() { return m_flags; }
    void SetFlags(IdentifierFlagBits flags) { m_flags = flags; }

    bool IsReassigned() const { return m_is_reassigned; }
    void SetIsReassigned(bool is_reassigned) { m_is_reassigned = is_reassigned; }

    const RC<AstExpression> &GetCurrentValue() const { return Unalias()->m_current_value; }
    void SetCurrentValue(const RC<AstExpression> &expr) { Unalias()->m_current_value = expr; }
    const SymbolTypePtr_t &GetSymbolType() const { return Unalias()->m_symbol_type; }
    void SetSymbolType(const SymbolTypePtr_t &symbol_type) { Unalias()->m_symbol_type = symbol_type; }

    const Array<GenericInstanceTypeInfo::Arg> &GetTemplateParams() const
        { return Unalias()->m_template_params; }

    void SetTemplateParams(const Array<GenericInstanceTypeInfo::Arg> &template_params)
        { Unalias()->m_template_params = template_params; }

    Identifier *Unalias() { return (m_aliasee != nullptr) ? m_aliasee : this; }
    const Identifier *Unalias() const { return (m_aliasee != nullptr) ? m_aliasee : this; }

private:
    std::string m_name;
    int m_index;
    int m_stack_location;
    mutable int m_usecount;
    IdentifierFlagBits m_flags;
    Identifier *m_aliasee;
    RC<AstExpression> m_current_value;
    SymbolTypePtr_t m_symbol_type;
    bool m_is_reassigned;

    Array<GenericInstanceTypeInfo::Arg> m_template_params;
};

} // namespace hyperion::compiler

#endif

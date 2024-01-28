#ifndef IDENTIFIER_HPP
#define IDENTIFIER_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <core/lib/String.hpp>
#include <Types.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

using IdentifierFlagBits = UInt;

enum IdentifierFlags : IdentifierFlagBits
{
    FLAG_NONE                   = 0x0,
    FLAG_CONST                  = 0x1,
    FLAG_ALIAS                  = 0x2,
    FLAG_MIXIN                  = 0x4,
    FLAG_GENERIC                = 0x8,
    FLAG_DECLARED_IN_FUNCTION   = 0x10,
    FLAG_PLACEHOLDER            = 0x20,
    FLAG_ACCESS_PRIVATE         = 0x40,
    FLAG_ACCESS_PUBLIC          = 0x80,
    FLAG_ACCESS_PROTECTED       = 0x100,
    FLAG_ARGUMENT               = 0x200,
    FLAG_REF                    = 0x400,
    FLAG_ENUM                   = 0x800,
    FLAG_MEMBER                 = 0x1000,
    FLAG_GENERIC_SUBSTITUTION   = 0x2000,
    FLAG_CONSTRUCTOR            = 0x4000,
    FLAG_CLASS                  = 0x8000,
    FLAG_FUNCTION               = 0x10000,
    FLAG_NATIVE                 = 0x20000,
    FLAG_TRAIT                  = 0x40000
};

class Identifier
{
public:
    Identifier(const String &name, int Index, IdentifierFlagBits flags, Identifier *aliasee = nullptr);
    Identifier(const Identifier &other);

    const String &GetName() const { return m_name; }
    int GetIndex() const { return Unalias()->m_index; }
    
    Int GetStackLocation() const
        { return Unalias()->m_stack_location; }

    void SetStackLocation(Int stack_location)
    {
        Identifier *unaliased = Unalias();
        AssertThrowMsg(unaliased->m_stack_location == -1, "Stack location already set, cannot set again");

        unaliased->m_stack_location = stack_location;
    }

    void IncUseCount() const
        { Unalias()->m_usecount++; }

    void DecUseCount() const
        { Unalias()->m_usecount--; }

    Int GetUseCount() const
        { return Unalias()->m_usecount; }

    IdentifierFlagBits GetFlags() const
        { return m_flags; }

    IdentifierFlagBits &GetFlags()
        { return m_flags; }

    void SetFlags(IdentifierFlagBits flags)
        { m_flags = flags; }

    bool IsReassigned() const
        { return m_is_reassigned; }

    void SetIsReassigned(bool is_reassigned)
        { m_is_reassigned = is_reassigned; }

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
    String              m_name;
    Int                 m_index;
    Int                 m_stack_location;
    mutable Int         m_usecount;
    IdentifierFlagBits  m_flags;
    Identifier          *m_aliasee;
    RC<AstExpression>   m_current_value;
    SymbolTypePtr_t     m_symbol_type;
    bool                m_is_reassigned;

    Array<GenericInstanceTypeInfo::Arg> m_template_params;
};

} // namespace hyperion::compiler

#endif

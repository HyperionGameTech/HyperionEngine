#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <core/containers/String.hpp>
#include <core/Types.hpp>

#include <string>
#include <memory>

namespace hyperion::compiler {

using IdentifierFlagBits = uint32;

enum IdentifierFlags : IdentifierFlagBits
{
    FLAG_NONE = 0x0,
    FLAG_CONST = 0x1,
    FLAG_ALIAS = 0x2,
    FLAG_MIXIN = 0x4,
    FLAG_GENERIC = 0x8,
    FLAG_DECLARED_IN_FUNCTION = 0x10,
    FLAG_PLACEHOLDER = 0x20,
    FLAG_ACCESS_PRIVATE = 0x40,
    FLAG_ACCESS_PUBLIC = 0x80,
    FLAG_ACCESS_PROTECTED = 0x100,
    FLAG_ARGUMENT = 0x200,
    FLAG_REF = 0x400,
    FLAG_ENUM = 0x800,
    FLAG_MEMBER = 0x1000,
    FLAG_GENERIC_SUBSTITUTION = 0x2000,
    FLAG_CONSTRUCTOR = 0x4000,
    FLAG_CLASS = 0x8000,
    FLAG_FUNCTION = 0x10000,
    FLAG_NATIVE = 0x20000,
    FLAG_TRAIT = 0x40000,
    FLAG_CLOSURE_PLACEHOLDER = 0x80000
};

class Identifier
{
public:
    Identifier(const String& name, int index, IdentifierFlagBits flags, Identifier* aliasee = nullptr);
    Identifier(const Identifier& other);

    const String& GetName() const
    {
        return m_name;
    }
    int GetIndex() const
    {
        return Unalias()->m_index;
    }

    int GetStackLocation() const
    {
        return Unalias()->m_stackLocation;
    }

    void SetStackLocation(int stackLocation)
    {
        Identifier* unaliased = Unalias();
        Assert(unaliased->m_stackLocation == -1, "Stack location already set, cannot set again");

        unaliased->m_stackLocation = stackLocation;
    }

    void IncUseCount() const
    {
        Unalias()->m_usecount++;
    }

    void DecUseCount() const
    {
        Unalias()->m_usecount--;
    }

    int GetUseCount() const
    {
        return Unalias()->m_usecount;
    }

    IdentifierFlagBits GetFlags() const
    {
        return m_flags;
    }

    IdentifierFlagBits& GetFlags()
    {
        return m_flags;
    }

    void SetFlags(IdentifierFlagBits flags)
    {
        m_flags = flags;
    }
    const RC<AstExpression>& GetCurrentValue() const
    {
        return Unalias()->m_currentValue;
    }
    void SetCurrentValue(const RC<AstExpression>& expr)
    {
        Unalias()->m_currentValue = expr;
    }
    const SymbolTypeRef& GetSymbolType() const
    {
        return Unalias()->m_symbolType;
    }
    void SetSymbolType(const SymbolTypeRef& symbolType)
    {
        Unalias()->m_symbolType = symbolType;
    }

    const Array<GenericInstanceTypeInfo::Arg>& GetTemplateParams() const
    {
        return Unalias()->m_templateParams;
    }

    void SetTemplateParams(const Array<GenericInstanceTypeInfo::Arg>& templateParams)
    {
        Unalias()->m_templateParams = templateParams;
    }

    Identifier* Unalias()
    {
        return (m_aliasee != nullptr) ? m_aliasee : this;
    }
    const Identifier* Unalias() const
    {
        return (m_aliasee != nullptr) ? m_aliasee : this;
    }

private:
    String m_name;
    int m_index;
    int m_stackLocation;
    mutable int m_usecount;
    IdentifierFlagBits m_flags;
    Identifier* m_aliasee;
    RC<AstExpression> m_currentValue;
    SymbolTypeRef m_symbolType;

    Array<GenericInstanceTypeInfo::Arg> m_templateParams;
};

} // namespace hyperion::compiler

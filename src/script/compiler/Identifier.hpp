#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <core/containers/String.hpp>
#include <core/Types.hpp>

#include <string>
#include <memory>

namespace hyperion {

class Scope;

using IdentifierFlagBits = uint32;

enum IdentifierFlags : IdentifierFlagBits
{
    FLAG_NONE = 0x0,
    FLAG_CONST = 0x1,
    FLAG_ALIAS = 0x2,
    FLAG_MODULE = 0x4,
    FLAG_GENERIC = 0x8,
    FLAG_DECLARED_IN_FUNCTION = 0x10,
    FLAG_PLACEHOLDER = 0x20,

    FLAG_ACCESS_PRIVATE = 0x40,
    FLAG_ACCESS_PUBLIC = 0x80,
    FLAG_ACCESS_PROTECTED = 0x100,

    FLAG_ARGUMENT = 0x200,
    FLAG_REF = 0x400,

    FLAG_MEMBER = 0x1000,
    FLAG_STATIC_MEMBER = 0x2000,
    FLAG_ENUM_MEMBER = 0x4000,

    FLAG_MEMBER_ALL = (FLAG_MEMBER | FLAG_STATIC_MEMBER | FLAG_ENUM_MEMBER),

    FLAG_CONSTRUCTOR = 0x8000,
    FLAG_FUNCTION = 0x10000,
    FLAG_EXTERN = 0x20000,
    FLAG_LAX = 0x80000, //!< except from many analyses, this identifier should be hidden from the user
    FLAG_PREREGISTER = 0x100000
};

class Identifier
{
public:
    Identifier(const String& name, int index, IdentifierFlagBits flags, Identifier* aliasee = nullptr);
    Identifier(const Identifier& other) = delete;
    Identifier& operator=(const Identifier& other) = delete;
    ~Identifier() = default;

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

    Identifier* Unalias()
    {
        return (m_aliasee != nullptr) ? m_aliasee : this;
    }

    const Identifier* Unalias() const
    {
        return (m_aliasee != nullptr) ? m_aliasee : this;
    }

    Scope* GetDeclScope() const
    {
        return Unalias()->m_declScope;
    }

    void SetDeclScope(Scope* scope)
    {
        Unalias()->m_declScope = scope;
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
    Scope* m_declScope;
};

} // namespace hyperion

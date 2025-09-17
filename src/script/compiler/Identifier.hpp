#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Scope;
class AstExpression;

enum class IdentifierFlags : uint32
{
    NONE = 0x0,
    CONSTANT = 0x1,
    ALIAS = 0x2,
    MODULE = 0x4,
    GENERIC = 0x8,
    DECLARED_IN_FUNCTION = 0x10,
    PLACEHOLDER = 0x20,

    ACCESS_PRIVATE = 0x40,
    ACCESS_PUBLIC = 0x80,
    ACCESS_PROTECTED = 0x100,

    ARGUMENT = 0x200,
    REF = 0x400,

    MEMBER = 0x1000,
    STATIC_MEMBER = 0x2000,
    ENUM_MEMBER = 0x4000,

    MEMBER_ALL = (MEMBER | STATIC_MEMBER | ENUM_MEMBER),

    CONSTRUCTOR = 0x8000,
    FUNCTION = 0x10000,
    EXTERN = 0x20000,
    LAX = 0x80000, //!< except from many analyses, this identifier should be hidden from the user
    PREREGISTER = 0x100000
};

HYP_MAKE_ENUM_FLAGS(IdentifierFlags)

class Identifier
{
public:
    Identifier(const String& name, int index, EnumFlags<IdentifierFlags> flags, Identifier* aliasee = nullptr);
    Identifier(const Identifier& other) = delete;
    Identifier& operator=(const Identifier& other) = delete;
    virtual ~Identifier() = default;

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

    HYP_FORCE_INLINE void IncUseCount() const
    {
        Unalias()->m_usecount++;
    }

    HYP_FORCE_INLINE void DecUseCount() const
    {
        Unalias()->m_usecount--;
    }

    HYP_FORCE_INLINE int GetUseCount() const
    {
        return Unalias()->m_usecount;
    }

    HYP_FORCE_INLINE EnumFlags<IdentifierFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE EnumFlags<IdentifierFlags>& GetFlags()
    {
        return m_flags;
    }

    HYP_FORCE_INLINE void SetFlags(EnumFlags<IdentifierFlags> flags)
    {
        m_flags = flags;
    }

    HYP_FORCE_INLINE const RC<AstExpression>& GetCurrentValue() const
    {
        return Unalias()->m_currentValue;
    }

    HYP_FORCE_INLINE void SetCurrentValue(const RC<AstExpression>& expr)
    {
        Unalias()->m_currentValue = expr;
    }

    HYP_FORCE_INLINE const SymbolType* GetSymbolType() const
    {
        return Unalias()->m_symbolType;
    }

    HYP_FORCE_INLINE void SetSymbolType(const SymbolType* symbolType)
    {
        Unalias()->m_symbolType = symbolType;
    }

    HYP_FORCE_INLINE Identifier* Unalias()
    {
        return (m_aliasee != nullptr) ? m_aliasee : this;
    }

    HYP_FORCE_INLINE const Identifier* Unalias() const
    {
        return (m_aliasee != nullptr) ? m_aliasee : this;
    }

    HYP_FORCE_INLINE Scope* GetDeclScope() const
    {
        return Unalias()->m_declScope;
    }

    HYP_FORCE_INLINE void SetDeclScope(Scope* scope)
    {
        Unalias()->m_declScope = scope;
    }

private:
    String m_name;
    int m_index;
    int m_stackLocation;
    mutable int m_usecount;
    EnumFlags<IdentifierFlags> m_flags;
    Identifier* m_aliasee;
    RC<AstExpression> m_currentValue;
    const SymbolType* m_symbolType;
    Scope* m_declScope;
};

} // namespace hyperion

#include <script/compiler/Identifier.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

namespace hyperion {

Identifier::Identifier(
    const String& name,
    int index,
    IdentifierFlagBits flags,
    Identifier* aliasee)
    : m_name(name),
      m_index(index),
      m_stackLocation(~0u),
      m_usecount(0),
      m_flags(flags),
      m_aliasee(aliasee),
      m_symbolType(BuiltinTypes::g_errorType)
{
}

Identifier::Identifier(const Identifier& other)
    : m_name(other.m_name),
      m_index(other.m_index),
      m_stackLocation(other.m_stackLocation),
      m_usecount(other.m_usecount),
      m_flags(other.m_flags),
      m_aliasee(other.m_aliasee),
      m_currentValue(other.m_currentValue),
      m_symbolType(other.m_symbolType)
{
}

} // namespace hyperion

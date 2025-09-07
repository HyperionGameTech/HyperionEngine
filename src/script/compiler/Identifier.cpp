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
      m_symbolType(BuiltinTypes::g_errorType),
      m_declScope(nullptr)
{
}

} // namespace hyperion

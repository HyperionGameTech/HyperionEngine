#include <script/compiler/Identifier.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

namespace Hyperion {

Identifier::Identifier(
    const String& name,
    int index,
    EnumFlags<IdentifierFlags> flags,
    Identifier* aliasee)
    : m_name(name),
      m_index(index),
      m_stackLocation(~0u),
      m_usecount(0),
      m_flags(flags),
      m_aliasee(aliasee),
      m_symbolType(BuiltinTypes::s_errorType),
      m_declScope(nullptr)
{
}

} // namespace Hyperion

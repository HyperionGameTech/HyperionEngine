#include <script/compiler/Identifier.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

namespace hyperion::compiler {

Identifier::Identifier(const std::string &name, int index, int flags, Identifier *aliasee)
    : m_name(name),
      m_index(index),
      m_stack_location(-1),
      m_usecount(0),
      m_flags(flags),
      m_aliasee(aliasee),
      m_symbol_type(BuiltinTypes::UNDEFINED),
      m_is_reassigned(false)
{
}

Identifier::Identifier(const Identifier &other)
    : m_name(other.m_name),
      m_index(other.m_index),
      m_stack_location(other.m_stack_location),
      m_usecount(other.m_usecount),
      m_flags(other.m_flags),
      m_aliasee(other.m_aliasee),
      m_current_value(other.m_current_value),
      m_symbol_type(other.m_symbol_type),
      m_is_reassigned(false)
{
}

} // namespace hyperion::compiler

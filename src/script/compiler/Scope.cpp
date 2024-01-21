#include <script/compiler/Scope.hpp>

namespace hyperion::compiler {

Scope::Scope()
    : m_scope_type(SCOPE_TYPE_NORMAL),
      m_scope_flags(0)
{
}

Scope::Scope(ScopeType scope_type, int scope_flags)
    : m_scope_type(scope_type),
      m_scope_flags(scope_flags)
{
}

Scope::Scope(const Scope &other)
    : m_identifier_table(other.m_identifier_table),
      m_scope_type(other.m_scope_type),
      m_scope_flags(other.m_scope_flags),
      m_return_types(other.m_return_types),
      m_closure_captures(other.m_closure_captures),
      m_generic_instance_cache(other.m_generic_instance_cache)
{
}

} // namespace hyperion::compiler

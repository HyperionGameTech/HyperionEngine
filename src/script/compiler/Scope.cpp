#include <script/compiler/Scope.hpp>

namespace hyperion {

Scope::Scope()
    : identifierTable(this),
      scopeType(SCOPE_TYPE_NORMAL),
      scopeFlags(0)
{
}

Scope::Scope(ScopeType scopeType, int scopeFlags)
    : identifierTable(this),
      scopeType(scopeType),
      scopeFlags(scopeFlags)
{
}

} // namespace hyperion

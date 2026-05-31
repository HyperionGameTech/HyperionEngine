#include <Lang/Compiler/Scope.hpp>

namespace Hyperion {

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

} // namespace Hyperion

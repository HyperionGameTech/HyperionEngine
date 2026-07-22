#include <Lang/Compiler/Ast/AstStatement.hpp>

#include <AstStatement.generated.inl>

namespace Hyperion {

const String AstStatement::s_unnamed = "<unnamed>";

Pool* AstStatement::GetAllocator()
{
    static Pool s_astNodePool { 64 * 1024, PF_DEFAULT };
    return &s_astNodePool;
}

AstStatement::AstStatement(const SourceLocation& location)
    : m_location(location),
      m_scopeDepth(0)
{
}

} // namespace Hyperion

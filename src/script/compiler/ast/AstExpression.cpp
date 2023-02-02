#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion::compiler {

AstExpression::AstExpression(
    const SourceLocation &location,
    int access_options
) : AstStatement(location),
    m_access_options(access_options),
    m_access_mode(ACCESS_MODE_LOAD)
{
}

} // namespace hyperion::compiler

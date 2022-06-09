#include <script/compiler/ast/AstStatement.hpp>

namespace hyperion::compiler {

AstStatement::AstStatement(const SourceLocation &location)
    : m_location(location)
{
}

} // namespace hyperion::compiler

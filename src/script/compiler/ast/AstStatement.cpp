#include <script/compiler/ast/AstStatement.hpp>

namespace hyperion::compiler {

const std::string AstStatement::unnamed = "<unnamed>";

AstStatement::AstStatement(const SourceLocation &location)
    : m_location(location)
{
}

} // namespace hyperion::compiler

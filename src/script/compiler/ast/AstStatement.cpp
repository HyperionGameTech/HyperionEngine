#include <script/compiler/ast/AstStatement.hpp>

namespace hyperion {
namespace compiler {

AstStatement::AstStatement(const SourceLocation &location)
    : m_location(location)
{
}

} // namespace compiler
} // namespace hyperion

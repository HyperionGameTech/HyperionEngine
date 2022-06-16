#include <script/compiler/AstVisitor.hpp>

namespace hyperion::compiler {

AstVisitor::AstVisitor(AstIterator *ast_iterator,
    CompilationUnit *compilation_unit)
    : m_ast_iterator(ast_iterator),
      m_compilation_unit(compilation_unit)
{
}

bool AstVisitor::Assert(bool expr, const CompilerError &error)
{
    if (!expr) {
        m_compilation_unit->GetErrorList().AddError(error);
        return false;
    }
    return true;
}

} // namespace hyperion::compiler

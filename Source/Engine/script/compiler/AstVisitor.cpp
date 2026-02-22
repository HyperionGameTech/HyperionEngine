#include <script/compiler/AstVisitor.hpp>

namespace Hyperion {

AstVisitor::AstVisitor(AstIterator* astIterator,
    CompilationUnit* compilationUnit)
    : m_astIterator(astIterator),
      m_compilationUnit(compilationUnit)
{
}

bool AstVisitor::AddErrorIfFalse(bool expr, const CompilerError& error)
{
    if (!expr)
    {
        m_compilationUnit->GetErrorList().AddError(error);
        return false;
    }
    return true;
}

void AstVisitor::ReportInternalError(const SourceLocation& location)
{
    m_compilationUnit->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_internal_error,
        location));
}

} // namespace Hyperion

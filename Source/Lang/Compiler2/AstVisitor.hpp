#pragma once

#include <Lang/compiler/AstIterator.hpp>
#include <Lang/compiler/ErrorList.hpp>
#include <Lang/compiler/CompilationUnit.hpp>

namespace Hyperion {

class CompilerError;

class AstVisitor
{
public:
    AstVisitor(
        AstIterator* astIterator,
        CompilationUnit* compilationUnit);
    virtual ~AstVisitor() = default;

    AstIterator* GetAstIterator() const
    {
        return m_astIterator;
    }
    CompilationUnit* GetCompilationUnit() const
    {
        return m_compilationUnit;
    }

    /** If expr is false, the given error is added to the error list. */
    bool AddErrorIfFalse(bool expr, const CompilerError& error);
    void ReportInternalError(const SourceLocation& location);

protected:
    AstIterator* m_astIterator;
    CompilationUnit* m_compilationUnit;
};

} // namespace Hyperion

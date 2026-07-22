#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>

#include <string>

namespace Hyperion {

HYP_CLASS()
class AstExportStatement : public AstStatement
{
    HYP_OBJECT_BODY(AstExportStatement);

public:
    AstExportStatement(
        const Handle<AstStatement>& stmt,
        const SourceLocation& location);
    virtual ~AstExportStatement() = default;

    const Handle<AstStatement>& GetStatement() const
    {
        return m_stmt;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstExportStatement>());
        hc.Add(m_stmt ? m_stmt->GetHashCode() : HashCode());

        return hc;
    }

private:
    Handle<AstStatement> m_stmt;

    // set while analyzing
    String m_exportedSymbolName;

    Handle<AstExportStatement> CloneImpl() const
    {
        return MakeHandle<AstExportStatement>(
            CloneAstNode(m_stmt),
            m_location);
    }
};

} // namespace Hyperion

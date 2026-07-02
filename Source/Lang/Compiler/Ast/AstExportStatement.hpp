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
        const SharedPtr<AstStatement>& stmt,
        const SourceLocation& location);
    virtual ~AstExportStatement() = default;

    const SharedPtr<AstStatement>& GetStatement() const
    {
        return m_stmt;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstExportStatement>());
        hc.Add(m_stmt ? m_stmt->GetHashCode() : HashCode());

        return hc;
    }

private:
    SharedPtr<AstStatement> m_stmt;

    // set while analyzing
    String m_exportedSymbolName;

    SharedPtr<AstExportStatement> CloneImpl() const
    {
        return SharedPtr<AstExportStatement>(new AstExportStatement(
            CloneAstNode(m_stmt),
            m_location));
    }
};

} // namespace Hyperion

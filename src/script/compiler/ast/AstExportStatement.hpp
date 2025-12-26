#pragma once

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>

#include <string>

namespace Hyperion {

class AstExportStatement : public AstStatement
{
public:
    AstExportStatement(
        const RC<AstStatement>& stmt,
        const SourceLocation& location);
    virtual ~AstExportStatement() = default;

    const RC<AstStatement>& GetStatement() const
    {
        return m_stmt;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstExportStatement>());
        hc.Add(m_stmt ? m_stmt->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstStatement> m_stmt;

    // set while analyzing
    String m_exportedSymbolName;

    RC<AstExportStatement> CloneImpl() const
    {
        return RC<AstExportStatement>(new AstExportStatement(
            CloneAstNode(m_stmt),
            m_location));
    }
};

} // namespace Hyperion

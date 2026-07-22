#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>

namespace Hyperion {

HYP_CLASS()
class AstReturnStatement final : public AstStatement
{
    HYP_OBJECT_BODY(AstReturnStatement);

public:
    AstReturnStatement(
        const Handle<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstReturnStatement() = default;

    const Handle<AstExpression>& GetExpression() const
    {
        return m_expr;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstReturnStatement>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

private:
    Handle<AstExpression> m_expr;

    // set while analyzing
    Handle<AstExpression> m_overrideExpr;
    const SymbolType* m_exprType;
    uint32 m_numPops;
    bool m_isConstructor : 1;

    Handle<AstReturnStatement> CloneImpl() const
    {
        return MakeHandle<AstReturnStatement>(
            CloneAstNode(m_expr),
            m_location);
    }
};

} // namespace Hyperion

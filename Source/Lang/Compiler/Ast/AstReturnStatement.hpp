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
        const SharedPtr<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstReturnStatement() = default;

    const SharedPtr<AstExpression>& GetExpression() const
    {
        return m_expr;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstReturnStatement>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

private:
    SharedPtr<AstExpression> m_expr;

    // set while analyzing
    SharedPtr<AstExpression> m_overrideExpr;
    const SymbolType* m_exprType;
    uint32 m_numPops;
    bool m_isConstructor : 1;

    SharedPtr<AstReturnStatement> CloneImpl() const
    {
        return SharedPtr<AstReturnStatement>(new AstReturnStatement(
            CloneAstNode(m_expr),
            m_location));
    }
};

} // namespace Hyperion

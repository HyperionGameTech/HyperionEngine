#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/HashCode.hpp>

#include <memory>

namespace Hyperion {

struct CaseClause
{
    RC<AstExpression> m_value;
    RC<AstBlock> m_block;
    bool m_isDefault = false;
};

HYP_CLASS()
class AstSwitchExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstSwitchExpression);

public:
    AstSwitchExpression(
        const RC<AstExpression>& expression,
        const Array<CaseClause>& clauses,
        const SourceLocation& location);

    virtual ~AstSwitchExpression() = default;

    HYP_FORCE_INLINE const RC<AstExpression>& GetExpression() const
    {
        return m_expression;
    }

    HYP_FORCE_INLINE const Array<CaseClause>& GetClauses() const
    {
        return m_clauses;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;
    virtual bool IsLiteral() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstSwitchExpression>());
        hc.Add(m_expression ? m_expression->GetHashCode() : HashCode());

        for (const auto& clause : m_clauses)
        {
            hc.Add(clause.m_value ? clause.m_value->GetHashCode() : HashCode());
            hc.Add(clause.m_block ? clause.m_block->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    RC<AstExpression> m_expression;
    Array<CaseClause> m_clauses;
    int m_numPops = 0;
    const SymbolType* m_exprType = nullptr;

    RC<AstSwitchExpression> CloneImpl() const;
};

} // namespace Hyperion

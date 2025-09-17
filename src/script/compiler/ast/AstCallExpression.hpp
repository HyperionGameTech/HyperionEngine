#pragma once

#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion {

class AstCallExpression : public AstExpression
{
public:
    AstCallExpression(
        const RC<AstExpression>& expr,
        const Array<RC<AstArgument>>& args,
        bool insertSelf,
        const SourceLocation& location);
    virtual ~AstCallExpression() = default;

    HYP_FORCE_INLINE Array<RC<AstArgument>>& GetArguments()
    {
        return m_args;
    }

    HYP_FORCE_INLINE const Array<RC<AstArgument>>& GetArguments() const
    {
        return m_args;
    }

    HYP_FORCE_INLINE const SymbolType* GetReturnType() const
    {
        return m_returnType;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;
    virtual AstExpression* GetTarget() const override;

    virtual String ToString() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstCallExpression>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        for (auto& arg : m_args)
        {
            hc.Add(arg ? arg->GetHashCode() : HashCode());
        }

        hc.Add(m_insertSelf);

        return hc;
    }

protected:
    RC<AstExpression> m_expr;
    Array<RC<AstArgument>> m_args;
    bool m_insertSelf;

    // set while analyzing
    RC<AstExpression> m_overrideExpr;
    Array<RC<AstArgument>> m_substitutedArgs;
    const SymbolType* m_returnType;

    RC<AstCallExpression> CloneImpl() const
    {
        return RC<AstCallExpression>(new AstCallExpression(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_args),
            m_insertSelf,
            m_location));
    }
};

} // namespace hyperion

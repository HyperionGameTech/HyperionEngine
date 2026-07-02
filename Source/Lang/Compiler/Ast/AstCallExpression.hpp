#pragma once

#include <Lang/Compiler/Ast/AstIdentifier.hpp>
#include <Lang/Compiler/Ast/AstArgumentList.hpp>

#include <string>
#include <vector>
#include <memory>

namespace Hyperion {

HYP_CLASS()
class AstCallExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstCallExpression);

public:
    AstCallExpression(
        const SharedPtr<AstExpression>& expr,
        const Array<SharedPtr<AstArgument>>& args,
        bool insertSelf,
        const SourceLocation& location);
    virtual ~AstCallExpression() = default;

    HYP_FORCE_INLINE Array<SharedPtr<AstArgument>>& GetArguments()
    {
        return m_args;
    }

    HYP_FORCE_INLINE const Array<SharedPtr<AstArgument>>& GetArguments() const
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

    virtual SharedPtr<AstStatement> Clone() const override;

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
    SharedPtr<AstExpression> m_expr;
    Array<SharedPtr<AstArgument>> m_args;
    bool m_insertSelf;

    // set while analyzing
    SharedPtr<AstExpression> m_overrideExpr;
    Array<SharedPtr<AstArgument>> m_substitutedArgs;
    const SymbolType* m_returnType;

    SharedPtr<AstCallExpression> CloneImpl() const
    {
        return SharedPtr<AstCallExpression>(new AstCallExpression(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_args),
            m_insertSelf,
            m_location));
    }
};

} // namespace Hyperion

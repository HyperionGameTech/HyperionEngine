#pragma once

#include <string>

#include <Lang/compiler/ast/AstExpression.hpp>
#include <Lang/compiler/ast/AstTypeSpecifier.hpp>
#include <Lang/compiler/ast/AstString.hpp>
#include <Lang/compiler/type-system/SymbolType.hpp>

#define HYP_SCRIPT_TYPEOF_RETURN_OBJECT 1

namespace Hyperion {

class AstTypeRef;

class AstTypeOfExpression : public AstTypeSpecifier
{
public:
    AstTypeOfExpression(
        const RC<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstTypeOfExpression() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual const SymbolType* GetExprType() const override;
    virtual const SymbolType* GetHeldType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

private:
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    RC<AstTypeRef> m_typeRef;
    const SymbolType* m_heldType;
#else
    RC<AstExpression> m_stringExpr;
#endif

    inline RC<AstTypeOfExpression> CloneImpl() const
    {
        return RC<AstTypeOfExpression>(new AstTypeOfExpression(
            CloneAstNode(m_expr),
            m_location));
    }
};

} // namespace Hyperion

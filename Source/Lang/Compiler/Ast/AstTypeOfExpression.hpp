#pragma once

#include <string>

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstString.hpp>
#include <Lang/Compiler/TypeSystem/SymbolType.hpp>

#define HYP_SCRIPT_TYPEOF_RETURN_OBJECT 1

namespace Hyperion {

class AstTypeRef;

HYP_CLASS()
class AstTypeOfExpression : public AstTypeSpecifier
{
    HYP_OBJECT_BODY(AstTypeOfExpression);

public:
    AstTypeOfExpression(
        const Handle<AstExpression>& expr,
        const SourceLocation& location);
    virtual ~AstTypeOfExpression() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual const SymbolType* GetExprType() const override;
    virtual const SymbolType* GetHeldType() const override;

    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;

private:
#if HYP_SCRIPT_TYPEOF_RETURN_OBJECT
    Handle<AstTypeRef> m_typeRef;
    const SymbolType* m_heldType;
#else
    Handle<AstExpression> m_stringExpr;
#endif

    inline Handle<AstTypeOfExpression> CloneImpl() const
    {
        return MakeHandle<AstTypeOfExpression>(
            CloneAstNode(m_expr),
            m_location);
    }
};

} // namespace Hyperion

#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion::compiler {

class AstVariableDeclaration;

using AstTemplateExpressionFlags = uint32;

enum AstTemplateExpressionFlagBits : AstTemplateExpressionFlags
{
    AST_TEMPLATE_EXPRESSION_FLAG_NONE = 0x0,
    AST_TEMPLATE_EXPRESSION_FLAG_NATIVE = 0x1
};

class AstTemplateExpression final : public AstExpression
{
public:
    AstTemplateExpression(
        const RC<AstExpression>& expr,
        const Array<RC<AstParameter>>& genericParams,
        const RC<AstTypeSpecifier>& returnTypeSpecification,
        const SourceLocation& location);
    AstTemplateExpression(
        const RC<AstExpression>& expr,
        const Array<RC<AstParameter>>& genericParams,
        const RC<AstTypeSpecifier>& returnTypeSpecification,
        AstTemplateExpressionFlags flags,
        const SourceLocation& location);
    virtual ~AstTemplateExpression() override = default;

    const Array<RC<AstParameter>>& GetGenericParameters() const
    {
        return m_genericParams;
    }
    const RC<AstExpression>& GetInnerExpression() const
    {
        return m_expr;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypeRef GetExprType() const override;
    virtual SymbolTypeRef GetHeldType() const override;
    virtual const AstExpression* GetValueOf() const override;
    virtual const AstExpression* GetDeepValueOf() const override;
    virtual const AstExpression* GetHeldGenericExpr() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTemplateExpression>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        for (auto& param : m_genericParams)
        {
            hc.Add(param ? param->GetHashCode() : HashCode());
        }

        hc.Add(m_returnTypeSpecification ? m_returnTypeSpecification->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstExpression> m_expr;
    Array<RC<AstParameter>> m_genericParams;
    RC<AstTypeSpecifier> m_returnTypeSpecification;
    AstTemplateExpressionFlags m_flags;

    // set while analyzing
    SymbolTypeRef m_symbolType;
    RC<AstBlock> m_block;
    bool m_isVisited = false;

    RC<AstTemplateExpression> CloneImpl() const
    {
        return RC<AstTemplateExpression>(new AstTemplateExpression(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_genericParams),
            CloneAstNode(m_returnTypeSpecification),
            m_flags,
            m_location));
    }
};

} // namespace hyperion::compiler

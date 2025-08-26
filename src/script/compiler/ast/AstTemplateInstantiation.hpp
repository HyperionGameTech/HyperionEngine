#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

namespace hyperion::compiler {

class AstVariableDeclaration;

// An expression to represent an instantiated expression
class AstTemplateInstantiationWrapper : public AstExpression
{
public:
    AstTemplateInstantiationWrapper(
        const RC<AstExpression>& expr,
        const Array<GenericInstanceTypeInfo::Arg>& genericArgs,
        const SourceLocation& location);
    virtual ~AstTemplateInstantiationWrapper() = default;

    const RC<AstExpression>& GetExpr() const
    {
        return m_expr;
    }

    const Array<GenericInstanceTypeInfo::Arg>& GetGenericArgs() const
    {
        return m_genericArgs;
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

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTemplateInstantiationWrapper>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        for (auto& arg : m_genericArgs)
        {
            hc.Add(arg.m_type ? arg.m_type->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    void MakeSymbolTypeGenericInstance(SymbolTypeRef& symbolType);

    RC<AstExpression> m_expr;
    Array<GenericInstanceTypeInfo::Arg> m_genericArgs;

    // set while analyzing
    SymbolTypeRef m_exprType;
    SymbolTypeRef m_heldType;

    RC<AstTemplateInstantiationWrapper> CloneImpl() const
    {
        return RC<AstTemplateInstantiationWrapper>(new AstTemplateInstantiationWrapper(
            CloneAstNode(m_expr),
            m_genericArgs,
            m_location));
    }
};

class AstTemplateInstantiation : public AstExpression
{
public:
    AstTemplateInstantiation(
        const RC<AstExpression>& expr,
        const Array<RC<AstArgument>>& genericArgs,
        const SourceLocation& location);
    virtual ~AstTemplateInstantiation() = default;

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
    virtual AstExpression* GetTarget() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTemplateInstantiation>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        for (auto& arg : m_genericArgs)
        {
            hc.Add(arg ? arg->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    RC<AstExpression> m_expr;
    Array<RC<AstArgument>> m_genericArgs;

    // set while analyzing
    RC<AstTemplateInstantiationWrapper> m_innerExpr;
    RC<AstBlock> m_block;
    RC<AstExpression> m_targetExpr;
    Array<RC<AstArgument>> m_substitutedArgs;
    SymbolTypeRef m_exprType;
    SymbolTypeRef m_heldType;
    bool m_isVisited = false;
    bool m_isNative = false;

    RC<AstTemplateInstantiation> CloneImpl() const
    {
        return RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_genericArgs),
            m_location));
    }
};

} // namespace hyperion::compiler

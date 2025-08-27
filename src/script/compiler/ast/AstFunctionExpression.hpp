#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <core/containers/String.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstClass;

class AstFunctionExpression : public AstExpression
{
public:
    AstFunctionExpression(
        const Array<RC<AstParameter>>& parameters,
        const RC<AstTypeSpecifier>& returnTypeSpecification,
        const RC<AstBlock>& block,
        const SourceLocation& location);

    virtual ~AstFunctionExpression() override = default;

    bool IsConstructorDefinition() const
    {
        return m_isConstructorDefinition;
    }

    void SetIsConstructorDefinition(bool isConstructorDefinition)
    {
        m_isConstructorDefinition = isConstructorDefinition;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypeRef GetExprType() const override;

    const SymbolTypeRef& GetReturnType() const
    {
        return m_returnType;
    }

    void SetReturnType(const SymbolTypeRef& returnType)
    {
        m_returnType = returnType;
    }

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstFunctionExpression>());

        for (auto& param : m_parameters)
        {
            hc.Add(param ? param->GetHashCode() : HashCode());
        }

        hc.Add(m_returnTypeSpecification ? m_returnTypeSpecification->GetHashCode() : HashCode());

        hc.Add(m_block ? m_block->GetHashCode() : HashCode());

        return hc;
    }

protected:
    Array<RC<AstParameter>> m_parameters;
    RC<AstTypeSpecifier> m_returnTypeSpecification;
    RC<AstBlock> m_block;

    bool m_isClosure;

    RC<AstParameter> m_closureSelfParam;
    RC<AstTypeSpecifier> m_functionTypeExpr;
    RC<AstClass> m_closureTypeExpr;
    RC<AstBlock> m_blockWithParameters;

    bool m_isConstructorDefinition;

    SymbolTypeRef m_symbolType;
    SymbolTypeRef m_returnType;

    int m_closureObjectLocation;

    int m_staticId;

    UniquePtr<Buildable> BuildFunctionBody(AstVisitor* visitor, Module* mod);
    RC<AstFunctionExpression> CloneImpl() const
    {
        return RC<AstFunctionExpression>(new AstFunctionExpression(
            CloneAllAstNodes(m_parameters),
            CloneAstNode(m_returnTypeSpecification),
            CloneAstNode(m_block),
            m_location));
    }
};

} // namespace hyperion::compiler

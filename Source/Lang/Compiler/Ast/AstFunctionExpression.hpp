#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstParameter.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Core/Containers/String.hpp>

#include <memory>
#include <vector>

namespace Hyperion {

class AstClass;

HYP_CLASS()
class AstFunctionExpression : public AstExpression
{
    HYP_OBJECT_BODY(AstFunctionExpression);

public:
    AstFunctionExpression(
        const Array<SharedPtr<AstParameter>>& parameters,
        const SharedPtr<AstTypeSpecifier>& returnTypeSpecification,
        const SharedPtr<AstBlock>& block,
        const SourceLocation& location);

    AstFunctionExpression(
        const Array<SharedPtr<AstParameter>>& parameters,
        const SharedPtr<AstTypeSpecifier>& returnTypeSpecification,
        const SharedPtr<AstBlock>& block,
        bool enableClosure,
        const SourceLocation& location);

    virtual ~AstFunctionExpression() override = default;

    HYP_FORCE_INLINE bool IsConstructorDefinition() const
    {
        return m_isConstructorDefinition;
    }

    HYP_FORCE_INLINE void SetIsConstructorDefinition(bool isConstructorDefinition)
    {
        m_isConstructorDefinition = isConstructorDefinition;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    HYP_FORCE_INLINE const SymbolType* GetReturnType() const
    {
        return m_returnType;
    }

    HYP_FORCE_INLINE void SetReturnType(const SymbolType* returnType)
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
    Array<SharedPtr<AstParameter>> m_parameters;
    SharedPtr<AstTypeSpecifier> m_returnTypeSpecification;
    SharedPtr<AstBlock> m_block;

    bool m_enableClosure;
    bool m_isClosure;

    SharedPtr<AstParameter> m_closureSelfParam;
    SharedPtr<AstBlock> m_closureBlock;
    SharedPtr<AstBlock> m_blockWithParameters;

    bool m_isConstructorDefinition;

    const SymbolType* m_symbolType;
    const SymbolType* m_returnType;

    int m_closureObjectLocation;

    int m_staticId;

    UniquePtr<Buildable> BuildFunctionBody(AstVisitor* visitor, Module* mod);
    SharedPtr<AstFunctionExpression> CloneImpl() const
    {
        return SharedPtr<AstFunctionExpression>(new AstFunctionExpression(
            CloneAllAstNodes(m_parameters),
            CloneAstNode(m_returnTypeSpecification),
            CloneAstNode(m_block),
            m_location));
    }
};

} // namespace Hyperion

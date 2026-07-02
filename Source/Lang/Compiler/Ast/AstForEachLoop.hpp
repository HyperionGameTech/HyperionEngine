#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

namespace Hyperion {

HYP_CLASS()
class AstForEachLoop : public AstStatement
{
    HYP_OBJECT_BODY(AstForEachLoop);

public:
    AstForEachLoop(
        const SharedPtr<AstVariableDeclaration>& varDecl,
        const SharedPtr<AstExpression>& iterable,
        const SharedPtr<AstBlock>& block,
        const SourceLocation& location);
    virtual ~AstForEachLoop() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

    virtual String ToString() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstForEachLoop>());
        hc.Add(m_varDecl ? m_varDecl->GetHashCode() : HashCode());
        hc.Add(m_iterable ? m_iterable->GetHashCode() : HashCode());
        hc.Add(m_block ? m_block->GetHashCode() : HashCode());

        return hc;
    }

private:
    SharedPtr<AstVariableDeclaration> m_varDecl;
    SharedPtr<AstExpression> m_iterable;
    SharedPtr<AstBlock> m_block;

    int m_numLocals;

    SharedPtr<AstForEachLoop> CloneImpl() const
    {
        return SharedPtr<AstForEachLoop>(new AstForEachLoop(
            CloneAstNode(m_varDecl),
            CloneAstNode(m_iterable),
            CloneAstNode(m_block),
            m_location));
    }
};

} // namespace Hyperion

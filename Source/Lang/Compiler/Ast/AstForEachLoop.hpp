#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

namespace Hyperion {

class AstForEachLoop : public AstStatement
{
public:
    AstForEachLoop(
        const RC<AstVariableDeclaration>& varDecl,
        const RC<AstExpression>& iterable,
        const RC<AstBlock>& block,
        const SourceLocation& location);
    virtual ~AstForEachLoop() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

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
    RC<AstVariableDeclaration> m_varDecl;
    RC<AstExpression> m_iterable;
    RC<AstBlock> m_block;

    int m_numLocals;

    RC<AstForEachLoop> CloneImpl() const
    {
        return RC<AstForEachLoop>(new AstForEachLoop(
            CloneAstNode(m_varDecl),
            CloneAstNode(m_iterable),
            CloneAstNode(m_block),
            m_location));
    }
};

} // namespace Hyperion

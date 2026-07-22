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
        const Handle<AstVariableDeclaration>& varDecl,
        const Handle<AstExpression>& iterable,
        const Handle<AstBlock>& block,
        const SourceLocation& location);
    virtual ~AstForEachLoop() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

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
    Handle<AstVariableDeclaration> m_varDecl;
    Handle<AstExpression> m_iterable;
    Handle<AstBlock> m_block;

    int m_numLocals;

    Handle<AstForEachLoop> CloneImpl() const
    {
        return MakeHandle<AstForEachLoop>(
            CloneAstNode(m_varDecl),
            CloneAstNode(m_iterable),
            CloneAstNode(m_block),
            m_location);
    }
};

} // namespace Hyperion

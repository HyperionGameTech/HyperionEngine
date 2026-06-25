#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

HYP_CLASS()
class AstWhileLoop : public AstStatement
{
    HYP_OBJECT_BODY(AstWhileLoop);

public:
    AstWhileLoop(
        const RC<AstExpression>& conditional,
        const RC<AstBlock>& block,
        const SourceLocation& location);
    virtual ~AstWhileLoop() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstWhileLoop>());
        hc.Add(m_conditional ? m_conditional->GetHashCode() : HashCode());
        hc.Add(m_block ? m_block->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstExpression> m_conditional;
    RC<AstBlock> m_block;

    // set while analyzing
    int m_numLocals;

    RC<AstWhileLoop> CloneImpl() const
    {
        return RC<AstWhileLoop>(new AstWhileLoop(
            CloneAstNode(m_conditional),
            CloneAstNode(m_block),
            m_location));
    }
};

} // namespace Hyperion

#pragma once

#include <Lang/compiler/ast/AstStatement.hpp>
#include <Lang/compiler/ast/AstExpression.hpp>
#include <Lang/compiler/ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

class AstIfStatement : public AstStatement
{
public:
    AstIfStatement(
        const RC<AstExpression>& conditional,
        const RC<AstBlock>& block,
        const RC<AstBlock>& elseBlock,
        const SourceLocation& location);
    virtual ~AstIfStatement() = default;

    HYP_FORCE_INLINE const RC<AstExpression>& GetConditional() const
    {
        return m_conditional;
    }

    HYP_FORCE_INLINE const RC<AstBlock>& GetBlock() const
    {
        return m_block;
    }

    HYP_FORCE_INLINE const RC<AstBlock>& GetElseBlock() const
    {
        return m_elseBlock;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual String ToString() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstIfStatement>());
        hc.Add(m_conditional ? m_conditional->GetHashCode() : HashCode());
        hc.Add(m_block ? m_block->GetHashCode() : HashCode());
        hc.Add(m_elseBlock ? m_elseBlock->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstExpression> m_conditional;
    RC<AstBlock> m_block;
    RC<AstBlock> m_elseBlock;

    RC<AstIfStatement> CloneImpl() const
    {
        return RC<AstIfStatement>(new AstIfStatement(
            CloneAstNode(m_conditional),
            CloneAstNode(m_block),
            CloneAstNode(m_elseBlock),
            m_location));
    }
};

} // namespace Hyperion

#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

HYP_CLASS()
class AstIfStatement : public AstStatement
{
    HYP_OBJECT_BODY(AstIfStatement);

public:
    AstIfStatement(
        const Handle<AstExpression>& conditional,
        const Handle<AstBlock>& block,
        const Handle<AstBlock>& elseBlock,
        const SourceLocation& location);
    virtual ~AstIfStatement() = default;

    HYP_FORCE_INLINE const Handle<AstExpression>& GetConditional() const
    {
        return m_conditional;
    }

    HYP_FORCE_INLINE const Handle<AstBlock>& GetBlock() const
    {
        return m_block;
    }

    HYP_FORCE_INLINE const Handle<AstBlock>& GetElseBlock() const
    {
        return m_elseBlock;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

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
    Handle<AstExpression> m_conditional;
    Handle<AstBlock> m_block;
    Handle<AstBlock> m_elseBlock;

    Handle<AstIfStatement> CloneImpl() const
    {
        return MakeHandle<AstIfStatement>(
            CloneAstNode(m_conditional),
            CloneAstNode(m_block),
            CloneAstNode(m_elseBlock),
            m_location);
    }
};

} // namespace Hyperion

#pragma once

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

namespace hyperion {

class AstForLoop : public AstStatement
{
public:
    AstForLoop(
        const RC<AstStatement>& declPart,
        const RC<AstExpression>& conditionPart,
        const RC<AstExpression>& incrementPart,
        const RC<AstBlock>& block,
        const SourceLocation& location);
    virtual ~AstForLoop() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual String ToString() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstForLoop>());
        hc.Add(m_declPart ? m_declPart->GetHashCode() : HashCode());
        hc.Add(m_conditionPart ? m_conditionPart->GetHashCode() : HashCode());
        hc.Add(m_incrementPart ? m_incrementPart->GetHashCode() : HashCode());
        hc.Add(m_block ? m_block->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstStatement> m_declPart;
    RC<AstExpression> m_conditionPart;
    RC<AstExpression> m_incrementPart;
    RC<AstBlock> m_block;

    // set while analyzing
    int m_numLocals;
    int m_numUsedInitializers;

    RC<AstExpression> m_expr;

    RC<AstForLoop> CloneImpl() const
    {
        return RC<AstForLoop>(new AstForLoop(
            CloneAstNode(m_declPart),
            CloneAstNode(m_conditionPart),
            CloneAstNode(m_incrementPart),
            CloneAstNode(m_block),
            m_location));
    }
};

} // namespace hyperion

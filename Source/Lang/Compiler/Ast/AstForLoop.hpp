#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>
#include <Lang/Compiler/Ast/AstFunctionExpression.hpp>

namespace Hyperion {

HYP_CLASS()
class AstForLoop : public AstStatement
{
    HYP_OBJECT_BODY(AstForLoop);

public:
    AstForLoop(
        const Handle<AstStatement>& declPart,
        const Handle<AstExpression>& conditionPart,
        const Handle<AstExpression>& incrementPart,
        const Handle<AstBlock>& block,
        const SourceLocation& location);
    virtual ~AstForLoop() override = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

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
    Handle<AstStatement> m_declPart;
    Handle<AstExpression> m_conditionPart;
    Handle<AstExpression> m_incrementPart;
    Handle<AstBlock> m_block;

    // set while analyzing
    int m_numLocals;
    int m_numUsedInitializers;

    Handle<AstExpression> m_expr;

    Handle<AstForLoop> CloneImpl() const
    {
        return MakeHandle<AstForLoop>(
            CloneAstNode(m_declPart),
            CloneAstNode(m_conditionPart),
            CloneAstNode(m_incrementPart),
            CloneAstNode(m_block),
            m_location);
    }
};

} // namespace Hyperion

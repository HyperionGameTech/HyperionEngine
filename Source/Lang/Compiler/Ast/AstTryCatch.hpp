#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

HYP_CLASS()
class AstTryCatch : public AstStatement
{
    HYP_OBJECT_BODY(AstTryCatch);

public:
    AstTryCatch(
        const Handle<AstBlock>& tryBlock,
        const Handle<AstBlock>& catchBlock,
        const SourceLocation& location);
    virtual ~AstTryCatch() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstTryCatch>());
        hc.Add(m_tryBlock ? m_tryBlock->GetHashCode() : HashCode());
        hc.Add(m_catchBlock ? m_catchBlock->GetHashCode() : HashCode());

        return hc;
    }

private:
    Handle<AstBlock> m_tryBlock;
    Handle<AstBlock> m_catchBlock;

    Handle<AstTryCatch> CloneImpl() const
    {
        return MakeHandle<AstTryCatch>(
            CloneAstNode(m_tryBlock),
            CloneAstNode(m_catchBlock),
            m_location);
    }
};

} // namespace Hyperion

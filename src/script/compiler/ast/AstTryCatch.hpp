#pragma once

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

class AstTryCatch : public AstStatement
{
public:
    AstTryCatch(
        const RC<AstBlock>& tryBlock,
        const RC<AstBlock>& catchBlock,
        const SourceLocation& location);
    virtual ~AstTryCatch() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstTryCatch>());
        hc.Add(m_tryBlock ? m_tryBlock->GetHashCode() : HashCode());
        hc.Add(m_catchBlock ? m_catchBlock->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstBlock> m_tryBlock;
    RC<AstBlock> m_catchBlock;

    RC<AstTryCatch> CloneImpl() const
    {
        return RC<AstTryCatch>(new AstTryCatch(
            CloneAstNode(m_tryBlock),
            CloneAstNode(m_catchBlock),
            m_location));
    }
};

} // namespace Hyperion

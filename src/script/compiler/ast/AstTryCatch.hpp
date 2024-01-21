#ifndef AST_TRY_CATCH_HPP
#define AST_TRY_CATCH_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion::compiler {

class AstTryCatch : public AstStatement
{
public:
    AstTryCatch(
        const RC<AstBlock> &try_block,
        const RC<AstBlock> &catch_block,
        const SourceLocation &location
    );
    virtual ~AstTryCatch() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstTryCatch>());
        hc.Add(m_try_block ? m_try_block->GetHashCode() : HashCode());
        hc.Add(m_catch_block ? m_catch_block->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstBlock>    m_try_block;
    RC<AstBlock>    m_catch_block;

    RC<AstTryCatch> CloneImpl() const
    {
        return RC<AstTryCatch>(new AstTryCatch(
            CloneAstNode(m_try_block),
            CloneAstNode(m_catch_block),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

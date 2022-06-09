#ifndef AST_TRY_CATCH_HPP
#define AST_TRY_CATCH_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion::compiler {

class AstTryCatch : public AstStatement {
public:
    AstTryCatch(const std::shared_ptr<AstBlock> &try_block,
        const std::shared_ptr<AstBlock> &catch_block,
        const SourceLocation &location);
    virtual ~AstTryCatch() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::shared_ptr<AstBlock> m_try_block;
    std::shared_ptr<AstBlock> m_catch_block;

    inline Pointer<AstTryCatch> CloneImpl() const
    {
        return Pointer<AstTryCatch>(new AstTryCatch(
            CloneAstNode(m_try_block),
            CloneAstNode(m_catch_block),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

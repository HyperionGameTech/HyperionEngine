#ifndef AST_ARRAY_ACCESS_HPP
#define AST_ARRAY_ACCESS_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Enums.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion::compiler {

class AstArrayAccess : public AstExpression {
public:
    AstArrayAccess(
        const std::shared_ptr<AstExpression> &target,
        const std::shared_ptr<AstExpression> &index,
        const SourceLocation &location
    );
    virtual ~AstArrayAccess() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual AstExpression *GetTarget() const override;
    virtual AstExpression *GetHeldGenericExpr() const override;
    virtual bool IsMutable() const override;

private:
    std::shared_ptr<AstExpression> m_target;
    std::shared_ptr<AstExpression> m_index;

    std::shared_ptr<AstArrayAccess> CloneImpl() const
    {
        return std::shared_ptr<AstArrayAccess>(new AstArrayAccess(
            CloneAstNode(m_target),
            CloneAstNode(m_index),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

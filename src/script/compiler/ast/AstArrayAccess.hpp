#ifndef AST_ARRAY_ACCESS_HPP
#define AST_ARRAY_ACCESS_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Enums.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion::compiler {

class AstArrayAccess : public AstExpression
{
public:
    AstArrayAccess(
        const RC<AstExpression> &target,
        const RC<AstExpression> &index,
        const SourceLocation &location
    );
    virtual ~AstArrayAccess() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual AstExpression *GetTarget() const override;
    virtual bool IsMutable() const override;

private:
    RC<AstExpression> m_target;
    RC<AstExpression> m_index;

    RC<AstArrayAccess> CloneImpl() const
    {
        return RC<AstArrayAccess>(new AstArrayAccess(
            CloneAstNode(m_target),
            CloneAstNode(m_index),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

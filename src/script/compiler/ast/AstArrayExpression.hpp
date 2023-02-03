#ifndef AST_ARRAY_EXPRESSION_HPP
#define AST_ARRAY_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstArrayExpression : public AstExpression {
public:
    AstArrayExpression(
        const Array<RC<AstExpression>> &members,
        const SourceLocation &location
    );
    virtual ~AstArrayExpression() = default;

    const Array<RC<AstExpression>> &GetMembers() const
        { return m_members; };

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

protected:
    Array<RC<AstExpression>> m_members;
    SymbolTypePtr_t m_held_type;

    RC<AstArrayExpression> CloneImpl() const
    {
        return RC<AstArrayExpression>(new AstArrayExpression(
            CloneAllAstNodes(m_members),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

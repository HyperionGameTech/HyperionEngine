#ifndef AST_TUPLE_EXPRESSION_HPP
#define AST_TUPLE_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstArgument.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstTupleExpression : public AstExpression {
public:
    AstTupleExpression(const std::vector<std::shared_ptr<AstArgument>> &members,
        const SourceLocation &location);
    virtual ~AstTupleExpression() = default;

    const std::vector<std::shared_ptr<AstArgument>> &GetMembers() const
        { return m_members; };

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual std::shared_ptr<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

protected:
    std::vector<std::shared_ptr<AstArgument>> m_members;
    
    // set while analyzing
    SymbolTypePtr_t m_symbol_type;

    std::shared_ptr<AstTupleExpression> CloneImpl() const
    {
        return std::shared_ptr<AstTupleExpression>(new AstTupleExpression(
            CloneAllAstNodes(m_members),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

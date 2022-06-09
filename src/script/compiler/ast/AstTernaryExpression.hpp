#ifndef AST_TERNARY_EXPRESSION_HPP
#define AST_TERNARY_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion {
namespace compiler {

class AstTernaryExpression : public AstExpression {
public:
    AstTernaryExpression(
        const std::shared_ptr<AstExpression> &conditional,
        const std::shared_ptr<AstExpression> &left,
        const std::shared_ptr<AstExpression> &right,
        const SourceLocation &location
    );
    virtual ~AstTernaryExpression() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;

private:
    std::shared_ptr<AstExpression> m_conditional;
    std::shared_ptr<AstExpression> m_left;
    std::shared_ptr<AstExpression> m_right;

    inline std::shared_ptr<AstTernaryExpression> CloneImpl() const
    {
        return std::shared_ptr<AstTernaryExpression>(
            new AstTernaryExpression(
                CloneAstNode(m_conditional),
                CloneAstNode(m_left),
                CloneAstNode(m_right),
                m_location
            ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif

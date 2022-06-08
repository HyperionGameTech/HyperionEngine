#ifndef AST_BINARY_EXPRESSION_HPP
#define AST_BINARY_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/Operator.hpp>
#include <script/compiler/Configuration.hpp>

namespace hyperion {
namespace compiler {

class AstBinaryExpression : public AstExpression {
public:
    AstBinaryExpression(const std::shared_ptr<AstExpression> &left,
        const std::shared_ptr<AstExpression> &right,
        const Operator *op,
        const SourceLocation &location);

    inline const std::shared_ptr<AstExpression> &GetLeft() const { return m_left; }
    inline const std::shared_ptr<AstExpression> &GetRight() const { return m_right; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;

private:
    std::shared_ptr<AstExpression> m_left;
    std::shared_ptr<AstExpression> m_right;
    const Operator *m_op;

#if ACE_ENABLE_LAZY_DECLARATIONS
    // if the expression is lazy declaration
    std::shared_ptr<AstVariableDeclaration> m_variable_declaration;
    std::shared_ptr<AstVariableDeclaration> CheckLazyDeclaration(AstVisitor *visitor, Module *mod);
#endif

    inline std::shared_ptr<AstBinaryExpression> CloneImpl() const
    {
        return std::shared_ptr<AstBinaryExpression>(
            new AstBinaryExpression(
                CloneAstNode(m_left),
                CloneAstNode(m_right),
                m_op,
                m_location
            ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif

#ifndef AST_RETURN_STATEMENT_HPP
#define AST_RETURN_STATEMENT_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion::compiler {

class AstReturnStatement : public AstStatement {
public:
    AstReturnStatement(const std::shared_ptr<AstExpression> &expr,
        const SourceLocation &location);
    virtual ~AstReturnStatement() = default;

    inline const std::shared_ptr<AstExpression> &GetExpression() const
        { return m_expr; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::shared_ptr<AstExpression> m_expr;
    int m_num_pops;

    inline Pointer<AstReturnStatement> CloneImpl() const
    {
        return Pointer<AstReturnStatement>(new AstReturnStatement(
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

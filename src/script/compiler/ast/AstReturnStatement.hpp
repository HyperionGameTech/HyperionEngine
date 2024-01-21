#ifndef AST_RETURN_STATEMENT_HPP
#define AST_RETURN_STATEMENT_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion::compiler {

class AstReturnStatement final : public AstStatement
{
public:
    AstReturnStatement(
        const RC<AstExpression> &expr,
        const SourceLocation &location
    );
    virtual ~AstReturnStatement() = default;

    const RC<AstExpression> &GetExpression() const
        { return m_expr; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstReturnStatement>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstExpression>   m_expr;

    // set while analyzing
    UInt                m_num_pops;
    Bool                m_is_visited = false;

    RC<AstReturnStatement> CloneImpl() const
    {
        return RC<AstReturnStatement>(new AstReturnStatement(
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

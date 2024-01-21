#ifndef AST_CONTINUE_STATEMENT_HPP
#define AST_CONTINUE_STATEMENT_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion::compiler {

class AstContinueStatement : public AstStatement
{
public:
    AstContinueStatement(const SourceLocation &location);
    virtual ~AstContinueStatement() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        return HashCode().Add(TypeName<AstContinueStatement>());
    }

private:
    UInt m_num_pops;

    RC<AstContinueStatement> CloneImpl() const
    {
        return RC<AstContinueStatement>(new AstContinueStatement(
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

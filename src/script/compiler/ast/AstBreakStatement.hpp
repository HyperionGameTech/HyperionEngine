#ifndef AST_BREAK_STATEMENT_HPP
#define AST_BREAK_STATEMENT_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>

#include <memory>

namespace hyperion::compiler {

class AstBreakStatement : public AstStatement
{
public:
    AstBreakStatement(const SourceLocation &location);
    virtual ~AstBreakStatement() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        return HashCode().Add(TypeName<AstBreakStatement>());
    }

private:
    UInt m_num_pops;

    RC<AstBreakStatement> CloneImpl() const
    {
        return RC<AstBreakStatement>(new AstBreakStatement(
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

class AstContinueStatement : public AstStatement
{
public:
    AstContinueStatement(const SourceLocation& location);
    virtual ~AstContinueStatement() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        return HashCode().Add(TypeName<AstContinueStatement>());
    }

private:
    uint32 m_numPops;

    RC<AstContinueStatement> CloneImpl() const
    {
        return RC<AstContinueStatement>(new AstContinueStatement(
            m_location));
    }
};

} // namespace Hyperion

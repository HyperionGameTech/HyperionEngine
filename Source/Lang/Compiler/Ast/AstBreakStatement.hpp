#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

class AstBreakStatement : public AstStatement
{
public:
    AstBreakStatement(const SourceLocation& location);
    virtual ~AstBreakStatement() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        return HashCode().Add(TypeName<AstBreakStatement>());
    }

private:
    uint32 m_numPops;

    RC<AstBreakStatement> CloneImpl() const
    {
        return RC<AstBreakStatement>(new AstBreakStatement(
            m_location));
    }
};

} // namespace Hyperion

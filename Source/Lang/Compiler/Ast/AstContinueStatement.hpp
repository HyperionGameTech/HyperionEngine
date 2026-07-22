#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>

#include <memory>

namespace Hyperion {

HYP_CLASS()
class AstContinueStatement : public AstStatement
{
    HYP_OBJECT_BODY(AstContinueStatement);

public:
    AstContinueStatement(const SourceLocation& location);
    virtual ~AstContinueStatement() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        return HashCode().Add(TypeName<AstContinueStatement>());
    }

private:
    uint32 m_numPops;

    Handle<AstContinueStatement> CloneImpl() const
    {
        return MakeHandle<AstContinueStatement>(
            m_location);
    }
};

} // namespace Hyperion

#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Operator.hpp>

#include <core/Types.hpp>

namespace hyperion {

class AstConstant : public AstExpression
{
public:
    AstConstant(const SourceLocation& location);
    virtual ~AstConstant() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override = 0;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual bool IsLiteral() const override
    {
        return true;
    }
    virtual RC<AstStatement> Clone() const override = 0;

    virtual Tribool IsTrue() const override = 0;
    virtual bool MayHaveSideEffects() const override;
    virtual bool IsNumber() const = 0;
    virtual hyperion::int32 IntValue() const = 0;
    virtual hyperion::uint32 UnsignedValue() const;
    virtual float FloatValue() const = 0;

    virtual HashCode GetHashCode() const override
    {
        return AstExpression::GetHashCode().Add(TypeName<AstConstant>());
    }

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const = 0;
};

} // namespace hyperion

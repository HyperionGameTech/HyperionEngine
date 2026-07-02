#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Operator.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

HYP_CLASS(Abstract)
class AstConstant : public AstExpression
{
    HYP_OBJECT_BODY(AstConstant);

protected:
    explicit AstConstant(const ConstantValue& constantValue, const SourceLocation& location);

public:
    virtual ~AstConstant() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override = 0;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual bool IsLiteral() const override final
    {
        return true;
    }

    HYP_FORCE_INLINE ConstantBitSize GetBitSize() const
    {
        return m_constantValue.bitSize;
    }

    virtual SharedPtr<AstStatement> Clone() const override = 0;

    virtual Tribool IsTrue() const override = 0;
    virtual bool MayHaveSideEffects() const override;

    virtual bool IsNumber() const = 0;

    virtual ConstantValue GetConstantValue() const override final
    {
        return m_constantValue;
    }

    virtual String ToString() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstExpression::GetHashCode()
            .Add(TypeName<AstConstant>())
            .Add(m_constantValue.GetHashCode());
    }

protected:
    ConstantValue m_constantValue;
};

} // namespace Hyperion

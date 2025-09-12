#pragma once

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/Operator.hpp>

#include <core/Types.hpp>

namespace hyperion {

class AstConstant : public AstExpression
{
protected:
    explicit AstConstant(const SourceLocation& location);
    AstConstant(ConstantBitSize bitSize, const SourceLocation& location);

public:
    virtual ~AstConstant() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override = 0;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual bool IsLiteral() const override
    {
        return true;
    }

    HYP_FORCE_INLINE ConstantBitSize GetBitSize() const
    {
        return m_bitSize;
    }

    virtual RC<AstStatement> Clone() const override = 0;

    virtual Tribool IsTrue() const override = 0;
    virtual bool MayHaveSideEffects() const override;

    virtual bool IsNumber() const = 0;

    virtual Optional<ConstantInt> IntValue() const override = 0;
    virtual Optional<ConstantUInt> UnsignedValue() const override;
    virtual Optional<ConstantFloat> FloatValue() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstExpression::GetHashCode()
            .Add(TypeName<AstConstant>())
            .Add(m_bitSize);
    }

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const = 0;

protected:
    ConstantBitSize m_bitSize;
};

} // namespace hyperion

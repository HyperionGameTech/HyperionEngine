#pragma once

#include <script/compiler/ast/AstConstant.hpp>

#include <cstdint>

namespace hyperion {

class AstUnsignedInteger : public AstConstant
{
public:
    AstUnsignedInteger(hyperion::uint64 value, ConstantBitSize bitSize, const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual hyperion::int64 IntValue() const override;
    virtual hyperion::uint64 UnsignedValue() const override;
    virtual double FloatValue() const override;

    virtual SymbolTypeRef GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstConstant::GetHashCode().Add(TypeName<AstUnsignedInteger>());
        hc.Add(m_value);

        return hc;
    }

private:
    hyperion::uint64 m_value;

    RC<AstUnsignedInteger> CloneImpl() const
    {
        return RC<AstUnsignedInteger>(new AstUnsignedInteger(
            m_value,
            m_bitSize,
            m_location));
    }
};

} // namespace hyperion

#pragma once

#include <script/compiler/ast/AstConstant.hpp>

#include <cstdint>

namespace hyperion {

class AstInteger : public AstConstant
{
public:
    AstInteger(
        hyperion::int64 value,
        ConstantBitSize bitSize,
        const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual Optional<int64> IntValue() const override;
    virtual Optional<uint64> UnsignedValue() const override;
    virtual Optional<double> FloatValue() const override;

    virtual SymbolTypeRef GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstConstant::GetHashCode().Add(TypeName<AstInteger>());
        hc.Add(m_value);

        return hc;
    }

    virtual String ToString() const override
    {
        return String::ToString(m_value);
    }

private:
    hyperion::int64 m_value;

    RC<AstInteger> CloneImpl() const
    {
        return RC<AstInteger>(new AstInteger(
            m_value,
            m_bitSize,
            m_location));
    }
};

} // namespace hyperion

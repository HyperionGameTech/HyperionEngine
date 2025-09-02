#pragma once

#include <script/compiler/ast/AstConstant.hpp>

#include <cstdint>

namespace hyperion::compiler {

class AstInteger : public AstConstant
{
public:
    AstInteger(hyperion::int32 value, const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::int32 IntValue() const override;
    virtual hyperion::uint32 UnsignedValue() const override;
    virtual float FloatValue() const override;
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
    hyperion::int32 m_value;

    RC<AstInteger> CloneImpl() const
    {
        return RC<AstInteger>(new AstInteger(
            m_value,
            m_location));
    }
};

} // namespace hyperion::compiler

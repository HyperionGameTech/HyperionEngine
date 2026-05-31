#pragma once

#include <Lang/compiler/ast/AstConstant.hpp>

#include <cstdint>

namespace Hyperion {

class AstUnsignedInteger final : public AstConstant
{
public:
    AstUnsignedInteger(uint64 value, ConstantBitSize bitSize, const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstConstant::GetHashCode().Add(TypeName<AstUnsignedInteger>());
    }

private:
    RC<AstUnsignedInteger> CloneImpl() const
    {
        return RC<AstUnsignedInteger>(new AstUnsignedInteger(
            m_constantValue.AsUInt(),
            m_constantValue.bitSize,
            m_location));
    }
};

} // namespace Hyperion

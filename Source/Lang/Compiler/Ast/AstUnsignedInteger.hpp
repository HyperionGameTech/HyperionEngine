#pragma once

#include <Lang/Compiler/Ast/AstConstant.hpp>

#include <cstdint>

namespace Hyperion {

HYP_CLASS()
class AstUnsignedInteger final : public AstConstant
{
    HYP_OBJECT_BODY(AstUnsignedInteger);

public:
    AstUnsignedInteger(uint64 value, ConstantBitSize bitSize, const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstConstant::GetHashCode().Add(TypeName<AstUnsignedInteger>());
    }

private:
    SharedPtr<AstUnsignedInteger> CloneImpl() const
    {
        return SharedPtr<AstUnsignedInteger>(new AstUnsignedInteger(
            m_constantValue.AsUInt(),
            m_constantValue.bitSize,
            m_location));
    }
};

} // namespace Hyperion

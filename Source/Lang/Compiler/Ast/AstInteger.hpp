#pragma once

#include <Lang/Compiler/Ast/AstConstant.hpp>

#include <cstdint>

namespace Hyperion {

class AstInteger final : public AstConstant
{
public:
    AstInteger(
        int64 value,
        ConstantBitSize bitSize,
        const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstConstant::GetHashCode().Add(TypeName<AstInteger>());
    }

private:
    RC<AstInteger> CloneImpl() const
    {
        return RC<AstInteger>(new AstInteger(
            m_constantValue.AsInt(),
            m_constantValue.bitSize,
            m_location));
    }
};

} // namespace Hyperion

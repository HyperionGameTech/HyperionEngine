#pragma once

#include <script/compiler/ast/AstConstant.hpp>

namespace Hyperion {

class AstFloat final : public AstConstant
{
public:
    AstFloat(
        double value,
        ConstantBitSize bitSize,
        const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstConstant::GetHashCode().Add(TypeName<AstFloat>());
    }

private:
    RC<AstFloat> CloneImpl() const
    {
        return RC<AstFloat>(new AstFloat(
            m_constantValue.AsFloat(),
            m_constantValue.bitSize,
            m_location));
    }
};

} // namespace Hyperion

#pragma once

#include <Lang/Compiler/Ast/AstConstant.hpp>

namespace Hyperion {

HYP_CLASS()
class AstFloat final : public AstConstant
{
    HYP_OBJECT_BODY(AstFloat);

public:
    AstFloat(
        double value,
        ConstantBitSize bitSize,
        const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstConstant::GetHashCode().Add(TypeName<AstFloat>());
    }

private:
    Handle<AstFloat> CloneImpl() const
    {
        return MakeHandle<AstFloat>(
            m_constantValue.AsFloat(),
            m_constantValue.bitSize,
            m_location);
    }
};

} // namespace Hyperion

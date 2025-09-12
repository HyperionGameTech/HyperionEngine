#pragma once

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion {

class AstFalse : public AstConstant
{
public:
    AstFalse(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual Optional<ConstantInt> IntValue() const override;
    virtual Optional<ConstantFloat> FloatValue() const override;

    virtual SymbolTypeRef GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const override;

    virtual HashCode GetHashCode() const override
    {
        return AstConstant::GetHashCode().Add(TypeName<AstFalse>());
    }

    virtual String ToString() const override
    {
        return "false";
    }

private:
    RC<AstFalse> CloneImpl() const
    {
        return RC<AstFalse>(new AstFalse(m_location));
    }
};

} // namespace hyperion

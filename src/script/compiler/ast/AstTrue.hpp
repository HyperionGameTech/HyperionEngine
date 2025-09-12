#pragma once

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion {

class AstTrue : public AstConstant
{
public:
    AstTrue(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual Optional<int64> IntValue() const override;
    virtual Optional<double> FloatValue() const override;

    virtual SymbolTypeRef GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const override;

    virtual HashCode GetHashCode() const override
    {
        return AstConstant::GetHashCode().Add(TypeName<AstTrue>());
    }

    virtual String ToString() const override
    {
        return "true";
    }

private:
    RC<AstTrue> CloneImpl() const
    {
        return RC<AstTrue>(new AstTrue(m_location));
    }
};

} // namespace hyperion

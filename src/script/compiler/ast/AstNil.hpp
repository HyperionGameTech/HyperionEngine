#pragma once

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion {

class AstNil : public AstConstant
{
public:
    AstNil(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual hyperion::int64 IntValue() const override;
    virtual double FloatValue() const override;

    virtual SymbolTypeRef GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const override;

private:
    RC<AstNil> CloneImpl() const
    {
        return RC<AstNil>(new AstNil(m_location));
    }
};

} // namespace hyperion

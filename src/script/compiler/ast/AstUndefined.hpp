#pragma once

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion {

class AstUndefined : public AstConstant
{
public:
    AstUndefined(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::int32 IntValue() const override;
    virtual float FloatValue() const override;
    virtual SymbolTypeRef GetExprType() const override;

    virtual RC<AstConstant> HandleOperator(Operators opType, const AstConstant* right) const override;

private:
    RC<AstUndefined> CloneImpl() const
    {
        return RC<AstUndefined>(new AstUndefined(
            m_location));
    }
};

} // namespace hyperion

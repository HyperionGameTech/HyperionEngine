#pragma once

#include <Lang/Compiler/Ast/AstConstant.hpp>

namespace Hyperion {

HYP_CLASS()
class AstUndefined : public AstConstant
{
    HYP_OBJECT_BODY(AstUndefined);

public:
    AstUndefined(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

private:
    RC<AstUndefined> CloneImpl() const
    {
        return RC<AstUndefined>(new AstUndefined(m_location));
    }
};

} // namespace Hyperion

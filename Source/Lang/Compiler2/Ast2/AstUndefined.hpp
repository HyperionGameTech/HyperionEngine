#pragma once

#include <Lang/compiler/ast/AstConstant.hpp>

namespace Hyperion {

class AstUndefined : public AstConstant
{
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

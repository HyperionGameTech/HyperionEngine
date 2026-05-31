#pragma once

#include <Lang/compiler/ast/AstConstant.hpp>

namespace Hyperion {

class AstNil final : public AstConstant
{
public:
    AstNil(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual String ToString() const override
    {
        return "null";
    }

private:
    RC<AstNil> CloneImpl() const
    {
        return RC<AstNil>(new AstNil(m_location));
    }
};

} // namespace Hyperion

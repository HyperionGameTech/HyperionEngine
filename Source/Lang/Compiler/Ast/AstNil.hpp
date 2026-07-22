#pragma once

#include <Lang/Compiler/Ast/AstConstant.hpp>

namespace Hyperion {

HYP_CLASS()
class AstNil final : public AstConstant
{
    HYP_OBJECT_BODY(AstNil);

public:
    AstNil(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual String ToString() const override
    {
        return "null";
    }

private:
    Handle<AstNil> CloneImpl() const
    {
        return MakeHandle<AstNil>(m_location);
    }
};

} // namespace Hyperion

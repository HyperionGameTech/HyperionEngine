#pragma once

#include <Lang/Compiler/Ast/AstConstant.hpp>

namespace Hyperion {

HYP_CLASS()
class AstFalse final : public AstConstant
{
    HYP_OBJECT_BODY(AstFalse);

public:
    AstFalse(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstConstant::GetHashCode().Add(TypeName<AstFalse>());
    }

    virtual String ToString() const override
    {
        return "false";
    }

private:
    Handle<AstFalse> CloneImpl() const
    {
        return MakeHandle<AstFalse>(m_location);
    }
};

} // namespace Hyperion

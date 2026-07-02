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

    virtual SharedPtr<AstStatement> Clone() const override;

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
    SharedPtr<AstFalse> CloneImpl() const
    {
        return SharedPtr<AstFalse>(new AstFalse(m_location));
    }
};

} // namespace Hyperion

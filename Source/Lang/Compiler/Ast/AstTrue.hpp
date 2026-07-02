#pragma once

#include <Lang/Compiler/Ast/AstConstant.hpp>

namespace Hyperion {

HYP_CLASS()
class AstTrue final : public AstConstant
{
    HYP_OBJECT_BODY(AstTrue);

public:
    AstTrue(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;

    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        return AstConstant::GetHashCode().Add(TypeName<AstTrue>());
    }

    virtual String ToString() const override
    {
        return "true";
    }

private:
    SharedPtr<AstTrue> CloneImpl() const
    {
        return SharedPtr<AstTrue>(new AstTrue(m_location));
    }
};

} // namespace Hyperion

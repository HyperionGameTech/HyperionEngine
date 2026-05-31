#pragma once

#include <Lang/compiler/ast/AstConstant.hpp>

namespace Hyperion {

class AstTrue final : public AstConstant
{
public:
    AstTrue(const SourceLocation& location);

    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

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
    RC<AstTrue> CloneImpl() const
    {
        return RC<AstTrue>(new AstTrue(m_location));
    }
};

} // namespace Hyperion

#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>

namespace Hyperion {

/*! \brief A reference to a type in the AST. Used for type specifiers, e.g. "int", "MyClass", etc. */
HYP_CLASS()
class AstTypeRef : public AstExpression
{
    HYP_OBJECT_BODY(AstTypeRef);

public:
    AstTypeRef(
        const SymbolType* symbolType,
        const SourceLocation& location);

    virtual ~AstTypeRef() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;

    virtual const SymbolType* GetExprType() const override;
    virtual const SymbolType* GetHeldType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTypeRef>());
        hc.Add(m_symbolType ? m_symbolType->GetHashCode() : HashCode());

        return hc;
    }

private:
    const SymbolType* m_symbolType;

    RC<AstTypeRef> CloneImpl() const
    {
        return RC<AstTypeRef>(new AstTypeRef(
            m_symbolType,
            m_location));
    }
};

} // namespace Hyperion

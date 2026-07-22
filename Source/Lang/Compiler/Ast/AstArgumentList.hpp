#pragma once

#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstArgument.hpp>
#include <Lang/Compiler/TypeSystem/SymbolType.hpp>

#include <string>

namespace Hyperion {

HYP_CLASS()
class AstArgumentList : public AstExpression
{
    HYP_OBJECT_BODY(AstArgumentList);

public:
    AstArgumentList(
        const Array<Handle<AstArgument>>& args,
        const SourceLocation& location);
    virtual ~AstArgumentList() = default;

    HYP_FORCE_INLINE Array<Handle<AstArgument>>& GetArguments()
    {
        return m_args;
    }

    HYP_FORCE_INLINE const Array<Handle<AstArgument>>& GetArguments() const
    {
        return m_args;
    }

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual Handle<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual const SymbolType* GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstArgumentList>());

        for (auto& arg : m_args)
        {
            hc.Add(arg ? arg->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    Array<Handle<AstArgument>> m_args;

    Handle<AstArgumentList> CloneImpl() const
    {
        return MakeHandle<AstArgumentList>(
            CloneAllAstNodes(m_args),
            m_location);
    }
};

} // namespace Hyperion

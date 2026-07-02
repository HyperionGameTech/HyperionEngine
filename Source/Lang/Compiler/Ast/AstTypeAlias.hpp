#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/TypeSystem/SymbolType.hpp>

#include <Core/Containers/String.hpp>

#include <string>
#include <memory>

namespace Hyperion {

HYP_CLASS()
class AstTypeAlias : public AstStatement
{
    HYP_OBJECT_BODY(AstTypeAlias);

public:
    AstTypeAlias(
        const String& name,
        const SharedPtr<AstTypeSpecifier>& aliasee,
        const SourceLocation& location);
    virtual ~AstTypeAlias() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstTypeAlias>());
        hc.Add(m_name);
        hc.Add(m_aliasee ? m_aliasee->GetHashCode() : HashCode());

        return hc;
    }

private:
    String m_name;
    SharedPtr<AstTypeSpecifier> m_aliasee;

    SharedPtr<AstTypeAlias> CloneImpl() const
    {
        return SharedPtr<AstTypeAlias>(new AstTypeAlias(
            m_name,
            CloneAstNode(m_aliasee),
            m_location));
    }
};

} // namespace Hyperion

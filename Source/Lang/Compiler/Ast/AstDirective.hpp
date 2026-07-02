#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstArrayExpression.hpp>
#include <Core/Containers/String.hpp>

#include <memory>
#include <string>

namespace Hyperion {

HYP_CLASS()
class AstDirective : public AstStatement
{
    HYP_OBJECT_BODY(AstDirective);

public:
    AstDirective(
        const String& key,
        const Array<String>& args,
        const SourceLocation& location);
    virtual ~AstDirective() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual SharedPtr<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc;
        hc.Add(TypeName<AstDirective>());
        hc.Add(m_key);

        for (auto& arg : m_args)
        {
            hc.Add(arg);
        }

        return hc;
    }

private:
    String m_key;
    Array<String> m_args;

    SharedPtr<AstDirective> CloneImpl() const
    {
        return SharedPtr<AstDirective>(new AstDirective(
            m_key,
            m_args,
            m_location));
    }
};

} // namespace Hyperion

#pragma once

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <core/containers/String.hpp>

#include <memory>
#include <string>

namespace hyperion {

class AstDirective : public AstStatement
{
public:
    AstDirective(
        const String& key,
        const Array<String>& args,
        const SourceLocation& location);
    virtual ~AstDirective() = default;

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

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

    RC<AstDirective> CloneImpl() const
    {
        return RC<AstDirective>(new AstDirective(
            m_key,
            m_args,
            m_location));
    }
};

} // namespace hyperion

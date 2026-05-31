#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstDeclaration.hpp>
#include <Core/Containers/String.hpp>

#include <vector>
#include <memory>

namespace Hyperion {

class AstModuleDeclaration : public AstDeclaration
{
public:
    AstModuleDeclaration(
        const String& name,
        const Array<RC<AstStatement>>& children,
        const SourceLocation& location);
    AstModuleDeclaration(const String& name, const SourceLocation& location);

    void AddChild(const RC<AstStatement>& child)
    {
        m_children.PushBack(child);
    }

    Array<RC<AstStatement>>& GetChildren()
    {
        return m_children;
    }

    const Array<RC<AstStatement>>& GetChildren() const
    {
        return m_children;
    }

    Module* GetModule() const
    {
        return m_module;
    }

    void PerformLookup(AstVisitor* visitor);

    virtual void Visit(AstVisitor* visitor, Module* mod) override;
    virtual UniquePtr<Buildable> Build(AstVisitor* visitor, Module* mod) override;
    virtual void Optimize(AstVisitor* visitor, Module* mod) override;

    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstDeclaration::GetHashCode().Add(TypeName<AstModuleDeclaration>());

        for (auto& child : m_children)
        {
            hc.Add(child ? child->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    /** Pre-register all class types for forward reference support */
    void PreRegisterClassTypes(AstVisitor* visitor, Module* mod);

    Array<RC<AstStatement>> m_children;
    Module* m_module;

    RC<AstModuleDeclaration> CloneImpl() const
    {
        return RC<AstModuleDeclaration>(new AstModuleDeclaration(
            m_name,
            CloneAllAstNodes(m_children),
            m_location));
    }
};

} // namespace Hyperion

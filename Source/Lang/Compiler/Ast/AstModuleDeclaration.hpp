#pragma once

#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstDeclaration.hpp>
#include <Core/Containers/String.hpp>

#include <vector>
#include <memory>

namespace Hyperion {

HYP_CLASS()
class AstModuleDeclaration : public AstDeclaration
{
    HYP_OBJECT_BODY(AstModuleDeclaration);

public:
    AstModuleDeclaration(
        const String& name,
        const Array<SharedPtr<AstStatement>>& children,
        const SourceLocation& location);
    AstModuleDeclaration(const String& name, const SourceLocation& location);

    void AddChild(const SharedPtr<AstStatement>& child)
    {
        m_children.PushBack(child);
    }

    Array<SharedPtr<AstStatement>>& GetChildren()
    {
        return m_children;
    }

    const Array<SharedPtr<AstStatement>>& GetChildren() const
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

    virtual SharedPtr<AstStatement> Clone() const override;

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

    Array<SharedPtr<AstStatement>> m_children;
    Module* m_module;

    SharedPtr<AstModuleDeclaration> CloneImpl() const
    {
        return SharedPtr<AstModuleDeclaration>(new AstModuleDeclaration(
            m_name,
            CloneAllAstNodes(m_children),
            m_location));
    }
};

} // namespace Hyperion

#ifndef AST_MODULE_DECLARATION_HPP
#define AST_MODULE_DECLARATION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstDeclaration.hpp>
#include <core/lib/String.hpp>

#include <vector>
#include <memory>

namespace hyperion::compiler {

class AstModuleDeclaration : public AstDeclaration
{
public:
    AstModuleDeclaration(
        const String &name, 
        const Array<RC<AstStatement>> &children,
        const SourceLocation &location
    );
    AstModuleDeclaration(const String &name, const SourceLocation &location);

    void AddChild(const RC<AstStatement> &child)
        { m_children.PushBack(child); }

    Array<RC<AstStatement>> &GetChildren()
        { return m_children; }

    const Array<RC<AstStatement>> &GetChildren() const
        { return m_children; }

    const RC<Module> &GetModule() const
        { return m_module; }

    void PerformLookup(AstVisitor *visitor);

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstDeclaration::GetHashCode().Add(TypeName<AstModuleDeclaration>());
        
        for (auto &child : m_children) {
            hc.Add(child ? child->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    Array<RC<AstStatement>> m_children;
    RC<Module>              m_module;

    RC<AstModuleDeclaration> CloneImpl() const
    {
        return RC<AstModuleDeclaration>(new AstModuleDeclaration(
            m_name, 
            CloneAllAstNodes(m_children), 
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

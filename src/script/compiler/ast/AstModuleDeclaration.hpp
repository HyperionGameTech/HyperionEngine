#ifndef AST_MODULE_DECLARATION_HPP
#define AST_MODULE_DECLARATION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstDeclaration.hpp>

#include <vector>
#include <memory>

namespace hyperion::compiler {

class AstModuleDeclaration : public AstDeclaration {
public:
    AstModuleDeclaration(const std::string &name, 
        const std::vector<RC<AstStatement>> &children,
        const SourceLocation &location);
    AstModuleDeclaration(const std::string &name, const SourceLocation &location);

    void AddChild(const RC<AstStatement> &child) { m_children.push_back(child); }
    std::vector<RC<AstStatement>> &GetChildren() { return m_children; }
    const std::vector<RC<AstStatement>> &GetChildren() const { return m_children; }

    const RC<Module> &GetModule() const { return m_module; }

    void PerformLookup(AstVisitor *visitor);

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

private:
    std::vector<RC<AstStatement>> m_children;
    RC<Module> m_module;

    void AddBuiltinHeader();

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

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
        const std::vector<std::shared_ptr<AstStatement>> &children,
        const SourceLocation &location);
    AstModuleDeclaration(const std::string &name, const SourceLocation &location);

    inline void AddChild(const std::shared_ptr<AstStatement> &child) { m_children.push_back(child); }
    inline std::vector<std::shared_ptr<AstStatement>> &GetChildren() { return m_children; }
    inline const std::vector<std::shared_ptr<AstStatement>> &GetChildren() const { return m_children; }

    inline const std::shared_ptr<Module> &GetModule() const { return m_module; }

    void PerformLookup(AstVisitor *visitor);

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::vector<std::shared_ptr<AstStatement>> m_children;
    std::shared_ptr<Module> m_module;

    void AddBuiltinHeader();

    inline Pointer<AstModuleDeclaration> CloneImpl() const
    {
        return Pointer<AstModuleDeclaration>(new AstModuleDeclaration(
            m_name, 
            CloneAllAstNodes(m_children), 
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

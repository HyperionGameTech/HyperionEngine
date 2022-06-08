#ifndef AST_MODULE_IMPORT_HPP
#define AST_MODULE_IMPORT_HPP

#include <script/compiler/ast/AstImport.hpp>

#include <string>

namespace hyperion {
namespace compiler {

class AstModuleImportPart : public AstStatement {
public:
    AstModuleImportPart(
      const std::string &left,
      const std::vector<std::shared_ptr<AstModuleImportPart>> &right_parts,
      const SourceLocation &location);
    virtual ~AstModuleImportPart() = default;

    inline const std::string &GetLeft() const { return m_left; }
    inline const std::vector<std::shared_ptr<AstModuleImportPart>> &GetParts() const
        { return m_right_parts; }

    inline void SetPullInModules(bool pull_in_modules) { m_pull_in_modules = pull_in_modules; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::string m_left;
    std::vector<std::shared_ptr<AstModuleImportPart>> m_right_parts;

    bool m_pull_in_modules;

    inline Pointer<AstModuleImportPart> CloneImpl() const
    {
        return Pointer<AstModuleImportPart>(new AstModuleImportPart(
            m_left,
            CloneAllAstNodes(m_right_parts),
            m_location
        ));
    }
};

class AstModuleImport : public AstImport {
public:
    AstModuleImport(
      const std::vector<std::shared_ptr<AstModuleImportPart>> &parts,
      const SourceLocation &location);

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::vector<std::shared_ptr<AstModuleImportPart>> m_parts;

    inline Pointer<AstModuleImport> CloneImpl() const
    {
        return Pointer<AstModuleImport>(new AstModuleImport(
            CloneAllAstNodes(m_parts),
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif

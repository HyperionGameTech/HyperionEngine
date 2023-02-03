#ifndef AST_MODULE_IMPORT_HPP
#define AST_MODULE_IMPORT_HPP

#include <script/compiler/ast/AstImport.hpp>

#include <string>

namespace hyperion::compiler {

class Identifier;
class AstModuleImportPart : public AstStatement
{
public:
    AstModuleImportPart(
        const std::string &left,
        const std::vector<RC<AstModuleImportPart>> &right_parts,
        const SourceLocation &location
    );
    virtual ~AstModuleImportPart() = default;

    const std::string &GetLeft() const { return m_left; }
    const std::vector<RC<AstModuleImportPart>> &GetParts() const
        { return m_right_parts; }

    void SetPullInModules(bool pull_in_modules)
        { m_pull_in_modules = pull_in_modules; }

    std::vector<RC<Identifier>> &GetIdentifiers()
        { return m_identifiers; }

    const std::vector<RC<Identifier>> &GetIdentifiers() const
        { return m_identifiers; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

private:
    std::string m_left;
    std::vector<RC<AstModuleImportPart>> m_right_parts;
    bool m_pull_in_modules;
    std::vector<RC<Identifier>> m_identifiers;

    RC<AstModuleImportPart> CloneImpl() const
    {
        return RC<AstModuleImportPart>(new AstModuleImportPart(
            m_left,
            CloneAllAstNodes(m_right_parts),
            m_location
        ));
    }
};

class AstModuleImport : public AstImport
{
public:
    AstModuleImport(
        const std::vector<RC<AstModuleImportPart>> &parts,
        const SourceLocation &location
    );

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

protected:
    std::vector<RC<AstModuleImportPart>> m_parts;

    RC<AstModuleImport> CloneImpl() const
    {
        return RC<AstModuleImport>(new AstModuleImport(
            CloneAllAstNodes(m_parts),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

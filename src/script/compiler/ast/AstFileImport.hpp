#ifndef AST_LOCAL_IMPORT_HPP
#define AST_LOCAL_IMPORT_HPP

#include <script/compiler/ast/AstImport.hpp>

#include <string>

namespace hyperion::compiler {

class AstFileImport : public AstImport {
public:
    AstFileImport(const std::string &path,
        const SourceLocation &location);

    const std::string &GetPath() const { return m_path; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::string m_path;

    Pointer<AstFileImport> CloneImpl() const
    {
        return Pointer<AstFileImport>(new AstFileImport(
            m_path,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

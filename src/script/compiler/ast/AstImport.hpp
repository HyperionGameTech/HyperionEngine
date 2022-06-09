#ifndef AST_IMPORT_HPP
#define AST_IMPORT_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/CompilationUnit.hpp>

namespace hyperion::compiler {

class AstImport : public AstStatement {
public:
    AstImport(const SourceLocation &location);
    virtual ~AstImport() = default;

    static void CopyModules(
        AstVisitor *visitor,
        Module *mod_to_copy,
        bool update_tree_link = false);

    static bool TryOpenFile(
        const std::string &path,
        std::ifstream &is);

    virtual void Visit(AstVisitor *visitor, Module *mod) override = 0;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override = 0;

protected:
    /** The AST iterator that will be used by the imported module */
    AstIterator m_ast_iterator;

    void PerformImport(
        AstVisitor *visitor,
        Module *mod,
        const std::string &filepath
        /*bool make_parent_module = false, // should we pull in all modules under a single one?
                                           // used for module-based imports, rather than includes.
        const std::string &parent_module_name = ""*/);
};

} // namespace hyperion::compiler

#endif

#ifndef AST_EXPORT_STATEMENT_HPP
#define AST_EXPORT_STATEMENT_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>

#include <string>

namespace hyperion::compiler {

class AstExportStatement : public AstStatement {
public:
    AstExportStatement(
        const std::shared_ptr<AstStatement> &stmt,
        const SourceLocation &location
    );
    virtual ~AstExportStatement() = default;

    inline const std::shared_ptr<AstStatement> &GetStatement() const
        { return m_stmt; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

private:
    std::shared_ptr<AstStatement> m_stmt;

    // set while analyzing
    std::string m_exported_symbol_name;

    inline Pointer<AstExportStatement> CloneImpl() const
    {
        return Pointer<AstExportStatement>(new AstExportStatement(
            CloneAstNode(m_stmt),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

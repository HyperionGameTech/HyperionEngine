#ifndef AST_DECLARATION_HPP
#define AST_DECLARATION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/Identifier.hpp>

#include <string>

namespace hyperion::compiler {

class AstDeclaration : public AstStatement {
public:
    AstDeclaration(const std::string &name,
        const SourceLocation &location);
    virtual ~AstDeclaration() = default;

    inline const std::string &GetName() const { return m_name; }
    inline void SetName(const std::string &name) { m_name = name; }
    inline Identifier *GetIdentifier() const { return m_identifier; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override = 0;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override = 0;
    
    virtual Pointer<AstStatement> Clone() const override = 0;

protected:
    std::string m_name;
    Identifier *m_identifier;
};

} // namespace hyperion::compiler

#endif

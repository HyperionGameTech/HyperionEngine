#ifndef AST_ALIAS_DECLARATION_HPP
#define AST_ALIAS_DECLARATION_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <memory>

namespace hyperion::compiler {

class AstAliasDeclaration : public AstDeclaration {
public:
    AstAliasDeclaration(const std::string &name,
        const std::shared_ptr<AstExpression> &aliasee,
        const SourceLocation &location);
    virtual ~AstAliasDeclaration() = default;

    inline const std::shared_ptr<AstExpression> &GetAliasee() const
        { return m_aliasee; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::shared_ptr<AstExpression> m_aliasee;

    inline Pointer<AstAliasDeclaration> CloneImpl() const
    {
        return Pointer<AstAliasDeclaration>(new AstAliasDeclaration(
            m_name,
            CloneAstNode(m_aliasee),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

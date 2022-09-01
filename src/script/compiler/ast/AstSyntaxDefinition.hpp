#ifndef AST_SYNTAX_DEFINITION_HPP
#define AST_SYNTAX_DEFINITION_HPP

#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstString.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstSyntaxDefinition : public AstStatement {
public:
    AstSyntaxDefinition(
        const std::shared_ptr<AstString> &syntax_string,
        const std::shared_ptr<AstString> &transform_string,
        const SourceLocation &location);
    virtual ~AstSyntaxDefinition() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::shared_ptr<AstString> m_syntax_string;
    std::shared_ptr<AstString> m_transform_string;

    Pointer<AstSyntaxDefinition> CloneImpl() const
    {
        return Pointer<AstSyntaxDefinition>(new AstSyntaxDefinition(
            CloneAstNode(m_syntax_string),
            CloneAstNode(m_transform_string),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

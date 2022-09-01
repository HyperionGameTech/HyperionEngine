#ifndef AST_FUNCTION_DEFINITION_HPP
#define AST_FUNCTION_DEFINITION_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstFunctionDefinition : public AstDeclaration {
public:
    AstFunctionDefinition(const std::string &name,
        const std::shared_ptr<AstFunctionExpression> &expr,
        const SourceLocation &location);
    virtual ~AstFunctionDefinition() = default;

    const std::shared_ptr<AstFunctionExpression> &GetAssignment() const
        { return m_expr; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

protected:
    std::shared_ptr<AstFunctionExpression> m_expr;

    Pointer<AstFunctionDefinition> CloneImpl() const
    {
        return Pointer<AstFunctionDefinition>(new AstFunctionDefinition(
            m_name,
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

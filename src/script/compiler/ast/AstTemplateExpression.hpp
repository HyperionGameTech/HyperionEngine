#ifndef AST_TEMPLATE_EXPRESSION_HPP
#define AST_TEMPLATE_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion::compiler {

class AstTemplateExpression : public AstExpression {
public:
    AstTemplateExpression(const std::shared_ptr<AstExpression> &expr,
        const std::vector<std::shared_ptr<AstParameter>> &generic_params,
        const std::shared_ptr<AstPrototypeSpecification> &return_type_specification,
        const SourceLocation &location);
    virtual ~AstTemplateExpression() = default;

    const std::vector<std::shared_ptr<AstParameter>> &GetGenericParameters() const { return m_generic_params; }
    const std::shared_ptr<AstExpression> &GetInnerExpression() const { return m_expr; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    virtual const AstExpression *GetHeldGenericExpr() const override;

private:
    std::shared_ptr<AstExpression> m_expr;
    std::vector<std::shared_ptr<AstParameter>> m_generic_params;
    std::shared_ptr<AstPrototypeSpecification> m_return_type_specification;

    // set while analyzing
    SymbolTypePtr_t m_symbol_type;

    Pointer<AstTemplateExpression> CloneImpl() const
    {
        return Pointer<AstTemplateExpression>(new AstTemplateExpression(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_generic_params),
            CloneAstNode(m_return_type_specification),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

#ifndef AST_TEMPLATE_EXPRESSION_HPP
#define AST_TEMPLATE_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion::compiler {

class AstVariableDeclaration;
class AstTypeObject;

using AstTemplateExpressionFlags = UInt32;

enum AstTemplateExpressionFlagBits : AstTemplateExpressionFlags
{
    AST_TEMPLATE_EXPRESSION_FLAG_NONE   = 0x0,
    AST_TEMPLATE_EXPRESSION_FLAG_NATIVE = 0x1
};

class AstTemplateExpression final : public AstExpression
{
public:
    AstTemplateExpression(
        const RC<AstExpression> &expr,
        const Array<RC<AstParameter>> &generic_params,
        const RC<AstPrototypeSpecification> &return_type_specification,
        const SourceLocation &location
    );
    AstTemplateExpression(
        const RC<AstExpression> &expr,
        const Array<RC<AstParameter>> &generic_params,
        const RC<AstPrototypeSpecification> &return_type_specification,
        AstTemplateExpressionFlags flags,
        const SourceLocation &location
    );
    virtual ~AstTemplateExpression() override = default;

    const Array<RC<AstParameter>> &GetGenericParameters() const { return m_generic_params; }
    const RC<AstExpression> &GetInnerExpression() const { return m_expr; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual SymbolTypePtr_t GetHeldType() const override;
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    virtual const AstExpression *GetHeldGenericExpr() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTemplateExpression>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        for (auto &param : m_generic_params) {
            hc.Add(param ? param->GetHashCode() : HashCode());
        }

        hc.Add(m_return_type_specification ? m_return_type_specification->GetHashCode() : HashCode());

        return hc;
    }

private:
    RC<AstExpression>                   m_expr;
    Array<RC<AstParameter>>             m_generic_params;
    RC<AstPrototypeSpecification>       m_return_type_specification;
    AstTemplateExpressionFlags          m_flags;

    // set while analyzing
    SymbolTypePtr_t                     m_symbol_type;
    RC<AstBlock>                        m_block;
    RC<AstTypeObject>                   m_native_dummy_type_object;
    Array<RC<AstTypeObject>>            m_generic_param_type_objects;
    bool                                m_is_visited = false;

    RC<AstTemplateExpression> CloneImpl() const
    {
        return RC<AstTemplateExpression>(new AstTemplateExpression(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_generic_params),
            CloneAstNode(m_return_type_specification),
            m_flags,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

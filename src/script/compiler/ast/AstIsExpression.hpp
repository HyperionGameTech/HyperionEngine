#ifndef AST_IS_EXPRESSION_HPP
#define AST_IS_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/Tribool.hpp>

#include <string>

namespace hyperion::compiler {

class AstIsExpression : public AstExpression
{
public:
    AstIsExpression(
        const RC<AstExpression> &target,
        const RC<AstPrototypeSpecification> &type_specification,
        const SourceLocation &location
    );

    virtual ~AstIsExpression() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstIsExpression>());
        hc.Add(m_target ? m_target->GetHashCode() : HashCode());
        hc.Add(m_type_specification ? m_type_specification->GetHashCode() : HashCode());

        return hc;
    }

protected:
    RC<AstExpression>               m_target;
    RC<AstPrototypeSpecification>   m_type_specification;

    // set while analyzing
    RC<AstExpression>               m_override_expr;

    Tribool                         m_is_type;

private:
    RC<AstIsExpression> CloneImpl() const
    {
        return RC<AstIsExpression>(new AstIsExpression(
            CloneAstNode(m_target),
            CloneAstNode(m_type_specification),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

#ifndef AST_ARRAY_EXPRESSION_HPP
#define AST_ARRAY_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstTypeObject;
class AstPrototypeSpecification;

class AstArrayExpression : public AstExpression
{
public:
    AstArrayExpression(
        const Array<RC<AstExpression>> &members,
        const SourceLocation &location
    );
    virtual ~AstArrayExpression() = default;

    const Array<RC<AstExpression>> &GetMembers() const
        { return m_members; };

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstArrayExpression>());

        for (auto &member : m_members) {
            hc.Add(member ? member->GetHashCode() : HashCode());
        }

        return hc;
    }

protected:
    Array<RC<AstExpression>>        m_members;

    // set while analyzing
    Array<RC<AstExpression>>        m_replaced_members;
    SymbolTypePtr_t                 m_held_type;
    SymbolTypePtr_t                 m_expr_type;
    RC<AstPrototypeSpecification>   m_array_type_expr;
    RC<AstExpression>               m_array_from_call;

    RC<AstArrayExpression> CloneImpl() const
    {
        return RC<AstArrayExpression>(new AstArrayExpression(
            CloneAllAstNodes(m_members),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

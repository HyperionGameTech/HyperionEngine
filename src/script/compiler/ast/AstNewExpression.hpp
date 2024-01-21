#ifndef AST_NEW_EXPRESSION_HPP
#define AST_NEW_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion::compiler {

class AstNewExpression : public AstExpression
{
public:
    AstNewExpression(
        const RC<AstPrototypeSpecification> &proto,
        const RC<AstArgumentList> &arg_list,
        bool enable_constructor_call,
        const SourceLocation &location
    );
    virtual ~AstNewExpression() override = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual AstExpression *GetTarget() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstNewExpression>());
        hc.Add(m_proto ? m_proto->GetHashCode() : HashCode());
        hc.Add(m_arg_list ? m_arg_list->GetHashCode() : HashCode());
        hc.Add(m_enable_constructor_call);

        return hc;
    }

private:
    RC<AstPrototypeSpecification>   m_proto;
    RC<AstArgumentList>             m_arg_list;
    Bool                            m_enable_constructor_call;

    /** Set while analyzing */
    RC<AstExpression>               m_object_value;
    SymbolTypePtr_t                 m_instance_type;
    SymbolTypePtr_t                 m_prototype_type;
    RC<AstBlock>                    m_constructor_block; // create a block to store temporary vars
    RC<AstExpression>               m_constructor_call;

    RC<AstNewExpression> CloneImpl() const
    {
        return RC<AstNewExpression>(new AstNewExpression(
            CloneAstNode(m_proto),
            CloneAstNode(m_arg_list),
            m_enable_constructor_call,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

#ifndef AST_TEMPLATE_INSTANTIATION_HPP
#define AST_TEMPLATE_INSTANTIATION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <vector>

namespace hyperion::compiler {

class AstVariableDeclaration;
class AstTypeObject;

// An expression to represent an instantiated expression
class AstTemplateInstantiationWrapper : public AstExpression
{
public:
    AstTemplateInstantiationWrapper(
        const RC<AstExpression> &expr,
        const Array<GenericInstanceTypeInfo::Arg> &generic_args,
        const SourceLocation &location
    );
    virtual ~AstTemplateInstantiationWrapper() = default;

    const RC<AstExpression> &GetExpr() const
        { return m_expr; }

    const Array<GenericInstanceTypeInfo::Arg> &GetGenericArgs() const
        { return m_generic_args; }

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

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTemplateInstantiationWrapper>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());

        for (auto &arg : m_generic_args) {
            hc.Add(arg.m_type ? arg.m_type->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    void MakeSymbolTypeGenericInstance(SymbolTypePtr_t &symbol_type);

    RC<AstExpression>                   m_expr;
    Array<GenericInstanceTypeInfo::Arg> m_generic_args;

    // set while analyzing
    SymbolTypePtr_t                     m_expr_type;
    SymbolTypePtr_t                     m_held_type;

    RC<AstTemplateInstantiationWrapper> CloneImpl() const
    {
        return RC<AstTemplateInstantiationWrapper>(new AstTemplateInstantiationWrapper(
            CloneAstNode(m_expr),
            m_generic_args,
            m_location
        ));
    }
};

class AstTemplateInstantiation : public AstExpression
{
public:
    AstTemplateInstantiation(
        const RC<AstExpression> &expr,
        const Array<RC<AstArgument>> &generic_args,
        const SourceLocation &location
    );
    virtual ~AstTemplateInstantiation() = default;

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
    virtual AstExpression *GetTarget() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstTemplateInstantiation>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());
        
        for (auto &arg : m_generic_args) {
            hc.Add(arg ? arg->GetHashCode() : HashCode());
        }

        return hc;
    }

private:
    RC<AstExpression>       m_expr;
    Array<RC<AstArgument>>  m_generic_args;

    // set while analyzing
    RC<AstTemplateInstantiationWrapper> m_inner_expr;
    RC<AstBlock>                        m_block;
    RC<AstExpression>                   m_target_expr;
    RC<AstTypeObject>                   m_type_object;
    Array<RC<AstArgument>>              m_substituted_args;
    SymbolTypePtr_t                     m_expr_type;
    SymbolTypePtr_t                     m_held_type;
    bool                                m_is_visited = false;
    bool                                m_is_native = false;

    RC<AstTemplateInstantiation> CloneImpl() const
    {
        return RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_generic_args),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

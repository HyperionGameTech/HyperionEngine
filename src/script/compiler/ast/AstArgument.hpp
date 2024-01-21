#ifndef AST_ARGUMENT_HPP
#define AST_ARGUMENT_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <core/lib/String.hpp>

#include <HashCode.hpp>

namespace hyperion::compiler {

class AstArgument : public AstExpression
{
public:
    AstArgument(
        const RC<AstExpression> &expr,
        bool is_splat,
        bool is_named,
        bool is_pass_by_ref,
        bool is_pass_const,
        const String &name,
        const SourceLocation &location
    );
    virtual ~AstArgument() override = default;

    const RC<AstExpression> &GetExpr() const
      { return m_expr; }

    bool IsSplat() const { return m_is_splat; }
    bool IsNamed() const { return m_is_named; }
    
    bool IsPassConst() const { return m_is_pass_const; }
    void SetIsPassConst(bool is_pass_const)
        { m_is_pass_const = is_pass_const; }

    bool IsPassByRef() const { return m_is_pass_by_ref; }
    void SetIsPassByRef(bool is_pass_by_ref)
        { m_is_pass_by_ref = is_pass_by_ref; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    //virtual const AstExpression *GetValueOf() const override { return m_expr.Get(); }

    virtual bool IsLiteral() const override;
    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    virtual const String &GetName() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstArgument>());
        hc.Add(m_expr ? m_expr->GetHashCode() : HashCode());
        hc.Add(m_is_splat);
        hc.Add(m_is_named);
        hc.Add(m_is_pass_by_ref);
        hc.Add(m_is_pass_const);
        hc.Add(m_name);

        return hc;
    }

private:
    RC<AstExpression>   m_expr;
    bool                m_is_splat;
    bool                m_is_named;
    bool                m_is_pass_by_ref;
    bool                m_is_pass_const;
    String              m_name;
    
    bool                m_is_visited = false;

    RC<AstArgument> CloneImpl() const
    {
        return RC<AstArgument>(new AstArgument(
            CloneAstNode(m_expr),
            m_is_splat,
            m_is_named,
            m_is_pass_by_ref,
            m_is_pass_const,
            m_name,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

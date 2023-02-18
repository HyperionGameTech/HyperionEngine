#ifndef AST_ARGUMENT_HPP
#define AST_ARGUMENT_HPP

#include <string>

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

namespace hyperion::compiler {

class AstArgument : public AstExpression
{
public:
    AstArgument(
        const RC<AstExpression> &expr,
        bool is_splat,
        bool is_named,
        const std::string &name,
        const SourceLocation &location
    );
    virtual ~AstArgument() = default;

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
    virtual const std::string &GetName() const override;

private:
    RC<AstExpression> m_expr;
    bool m_is_splat;
    bool m_is_named;
    bool m_is_pass_by_ref;
    bool m_is_pass_const;
    std::string m_name;
    
    bool m_is_visited = false;

    RC<AstArgument> CloneImpl() const
    {
        return RC<AstArgument>(new AstArgument(
            CloneAstNode(m_expr),
            m_is_splat,
            m_is_named,
            m_name,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

#ifndef AST_ARGUMENT_HPP
#define AST_ARGUMENT_HPP

#include <string>

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

namespace hyperion::compiler {

class AstArgument : public AstExpression {
public:
    AstArgument(
        const std::shared_ptr<AstExpression> &expr,
        bool is_splat,
        bool is_named,
        const std::string &name,
        const SourceLocation &location);
    virtual ~AstArgument() = default;

    inline const std::shared_ptr<AstExpression> &GetExpr() const
      { return m_expr; }

    inline bool IsSplat() const { return m_is_splat; }
    inline bool IsNamed() const { return m_is_named; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    //virtual const AstExpression *GetValueOf() const override { return m_expr.get(); }

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual const std::string &GetName() const override;

private:
    std::shared_ptr<AstExpression> m_expr;
    bool m_is_splat;
    bool m_is_named;
    std::string m_name;

    inline Pointer<AstArgument> CloneImpl() const
    {
        return Pointer<AstArgument>(new AstArgument(
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

#ifndef AST_TYPEOF_EXPRESSION
#define AST_TYPEOF_EXPRESSION

#include <string>

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

namespace hyperion {
namespace compiler {

class AstTypeOfExpression : public AstExpression {
public:
    AstTypeOfExpression(
      const std::shared_ptr<AstExpression> &expr,
      const SourceLocation &location);
    virtual ~AstTypeOfExpression() = default;

    inline const std::shared_ptr<AstExpression> &GetExpr() const
      { return m_expr; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;
    
private:
    std::shared_ptr<AstExpression> m_expr;

    std::shared_ptr<AstExpression> m_runtime_typeof_call;

    // set while compiling
    int m_static_id;

    inline Pointer<AstTypeOfExpression> CloneImpl() const
    {
        return Pointer<AstTypeOfExpression>(new AstTypeOfExpression(
            CloneAstNode(m_expr),
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif

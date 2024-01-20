#ifndef AST_CALL_EXPRESSION_HPP
#define AST_CALL_EXPRESSION_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion::compiler {

class AstCallExpression : public AstExpression
{
public:
    AstCallExpression(
        const RC<AstExpression> &expr,
        const Array<RC<AstArgument>> &args,
        bool insert_self,
        const SourceLocation &location
    );
    virtual ~AstCallExpression() = default;

    const Array<RC<AstArgument>> &GetArguments() const
        { return m_args; }
    
    const SymbolTypePtr_t &GetReturnType() const
        { return m_return_type; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual AstExpression *GetTarget() const override;

protected:
    RC<AstExpression>       m_expr;
    Array<RC<AstArgument>>  m_args;
    bool                    m_insert_self;

    // set while analyzing
    RC<AstExpression>       m_override_expr;
    Array<RC<AstArgument>>  m_substituted_args;
    SymbolTypePtr_t         m_return_type;
    bool                    m_is_visited = false;

    RC<AstCallExpression> CloneImpl() const
    {
        return RC<AstCallExpression>(new AstCallExpression(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_args),
            m_insert_self,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

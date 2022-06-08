#ifndef AST_ACTION_EXPRESSION_HPP
#define AST_ACTION_EXPRESSION_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstMember.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion {
namespace compiler {

class AstActionExpression : public AstExpression {
public:
    AstActionExpression(const std::vector<std::shared_ptr<AstArgument>> &actions,
        const std::shared_ptr<AstExpression> &target,
        const SourceLocation &location);
    virtual ~AstActionExpression() = default;
    
    inline const SymbolTypePtr_t &GetReturnType() const
        { return m_return_type; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;

protected:
    std::vector<std::shared_ptr<AstArgument>> m_actions;
    std::shared_ptr<AstExpression> m_target;

    // set while analyzing
    bool m_is_method_call;
    int m_member_found;
    std::vector<int> m_arg_ordering;
    SymbolTypePtr_t m_return_type;
    std::shared_ptr<AstExpression> m_expr;

    inline Pointer<AstActionExpression> CloneImpl() const
    {
        return Pointer<AstActionExpression>(new AstActionExpression(
            CloneAllAstNodes(m_actions),
            CloneAstNode(m_target),
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif

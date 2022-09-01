#ifndef AST_CALL_EXPRESSION_HPP
#define AST_CALL_EXPRESSION_HPP

#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>

#include <string>
#include <vector>
#include <memory>

namespace hyperion::compiler {

class AstCallExpression : public AstExpression {
public:
    AstCallExpression(
        const std::shared_ptr<AstExpression> &target,
        const std::vector<std::shared_ptr<AstArgument>> &args,
        bool insert_self,
        const SourceLocation &location);
    virtual ~AstCallExpression() = default;

    void AddArgumentToFront(const std::shared_ptr<AstArgument> &arg)
        { m_args.insert(m_args.begin(), arg); }
    void AddArgument(const std::shared_ptr<AstArgument> &arg)
        { m_args.push_back(arg); }
    const std::vector<std::shared_ptr<AstArgument>> &GetArguments() const
        { return m_args; }
    
    const SymbolTypePtr_t &GetReturnType() const
        { return m_return_type; }

    bool IsMethodCall() const
        { return m_is_method_call; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual AstExpression *GetTarget() const override;

protected:
    std::shared_ptr<AstExpression> m_target;
    std::vector<std::shared_ptr<AstArgument>> m_args;
    bool m_insert_self;

    // set while analyzing
    std::vector<std::shared_ptr<AstArgument>> m_substituted_args;
    SymbolTypePtr_t m_return_type;
    bool m_is_method_call;

    Pointer<AstCallExpression> CloneImpl() const
    {
        return Pointer<AstCallExpression>(new AstCallExpression(
            CloneAstNode(m_target),
            CloneAllAstNodes(m_args),
            m_insert_self,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

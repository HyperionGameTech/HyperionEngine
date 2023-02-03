#ifndef AST_ARGUMENT_LIST_HPP
#define AST_ARGUMENT_LIST_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion::compiler {

class AstArgumentList : public AstExpression {
public:
    AstArgumentList(const std::vector<RC<AstArgument>> &args,
      const SourceLocation &location);
    virtual ~AstArgumentList() = default;

    const std::vector<RC<AstArgument>> &GetArguments() const
      { return m_args; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    
private:
    std::vector<RC<AstArgument>> m_args;

    RC<AstArgumentList> CloneImpl() const
    {
        return RC<AstArgumentList>(new AstArgumentList(
            CloneAllAstNodes(m_args),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

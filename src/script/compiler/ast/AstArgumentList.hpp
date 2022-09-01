#ifndef AST_ARGUMENT_LIST_HPP
#define AST_ARGUMENT_LIST_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion::compiler {

class AstArgumentList : public AstExpression {
public:
    AstArgumentList(const std::vector<std::shared_ptr<AstArgument>> &args,
      const SourceLocation &location);
    virtual ~AstArgumentList() = default;

    const std::vector<std::shared_ptr<AstArgument>> &GetArguments() const
      { return m_args; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    
private:
    std::vector<std::shared_ptr<AstArgument>> m_args;

    Pointer<AstArgumentList> CloneImpl() const
    {
        return Pointer<AstArgumentList>(new AstArgumentList(
            CloneAllAstNodes(m_args),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

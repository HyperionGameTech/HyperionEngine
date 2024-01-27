#ifndef AST_ARGUMENT_LIST_HPP
#define AST_ARGUMENT_LIST_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion::compiler {

class AstArgumentList : public AstExpression
{
public:
    AstArgumentList(
      const Array<RC<AstArgument>> &args,
      const SourceLocation &location
    );
    virtual ~AstArgumentList() = default;

    Array<RC<AstArgument>> &GetArguments()
        { return m_args; }

    const Array<RC<AstArgument>> &GetArguments() const
        { return m_args; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstArgumentList>());
        
        for (auto &arg : m_args) {
            hc.Add(arg ? arg->GetHashCode() : HashCode());
        }

        return hc;
    }
    
private:
    Array<RC<AstArgument>> m_args;

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

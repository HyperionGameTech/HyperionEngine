#ifndef AST_NEW_EXPRESSION_HPP
#define AST_NEW_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstTypeSpecification.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion {
namespace compiler {

class AstNewExpression: public AstExpression {
public:
    AstNewExpression(
        const std::shared_ptr<AstTypeSpecification> &type_expr,
        const std::shared_ptr<AstArgumentList> &arg_list,
        const SourceLocation &location);
    virtual ~AstNewExpression() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;

private:
    std::shared_ptr<AstTypeSpecification> m_type_expr;
    std::shared_ptr<AstArgumentList> m_arg_list;

    /** Set while analyzing */
    std::shared_ptr<AstExpression> m_object_value;
    std::shared_ptr<AstCallExpression> m_constructor_call;

    inline Pointer<AstNewExpression> CloneImpl() const
    {
        return Pointer<AstNewExpression>(new AstNewExpression(
            CloneAstNode(m_type_expr),
            CloneAstNode(m_arg_list),
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif

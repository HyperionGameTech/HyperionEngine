#ifndef AST_TEMPLATE_INSTANTIATION_HPP
#define AST_TEMPLATE_INSTANTIATION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>
#include <vector>

namespace hyperion::compiler {

class AstVariableDeclaration;

class AstTemplateInstantiation : public AstExpression
{
public:
    AstTemplateInstantiation(
        const RC<AstIdentifier> &expr,
        const Array<RC<AstArgument>> &generic_args,
        const SourceLocation &location
    );
    virtual ~AstTemplateInstantiation() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;

private:
    RC<AstIdentifier> m_expr;
    Array<RC<AstArgument>> m_generic_args;

    // set while analyzing
    RC<AstExpression> m_inner_expr;
    RC<AstBlock> m_block;
    SymbolTypePtr_t m_expr_type;
    bool m_is_visited = false;

    RC<AstTemplateInstantiation> CloneImpl() const
    {
        return RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_generic_args),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

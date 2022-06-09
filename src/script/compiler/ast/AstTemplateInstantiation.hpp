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
class AstAliasDeclaration;

class AstTemplateInstantiation : public AstExpression {
public:
    AstTemplateInstantiation(const std::shared_ptr<AstExpression> &expr,
        const std::vector<std::shared_ptr<AstArgument>> &generic_args,
        const SourceLocation &location);
    virtual ~AstTemplateInstantiation() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

private:
    std::shared_ptr<AstExpression> m_expr;
    std::vector<std::shared_ptr<AstArgument>> m_generic_args;

    // set while analyzing
    //std::shared_ptr<AstExpression> m_inst_expr;
    //std::vector<std::shared_ptr<AstAliasDeclaration>> m_mixin_overrides;
    //std::vector<std::shared_ptr<AstVariableDeclaration>> m_param_overrides;
    std::shared_ptr<AstBlock> m_block;
    SymbolTypePtr_t m_return_type;

    inline Pointer<AstTemplateInstantiation> CloneImpl() const
    {
        return Pointer<AstTemplateInstantiation>(new AstTemplateInstantiation(
            CloneAstNode(m_expr),
            CloneAllAstNodes(m_generic_args),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

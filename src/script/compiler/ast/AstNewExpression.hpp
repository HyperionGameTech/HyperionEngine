#ifndef AST_NEW_EXPRESSION_HPP
#define AST_NEW_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <string>

namespace hyperion::compiler {

class AstNewExpression : public AstExpression {
public:
    AstNewExpression(
        const std::shared_ptr<AstPrototypeSpecification> &proto,
        const std::shared_ptr<AstArgumentList> &arg_list,
        bool enable_constructor_call,
        const SourceLocation &location
    );
    virtual ~AstNewExpression() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    virtual AstExpression *GetTarget() const override;

private:
    std::shared_ptr<AstPrototypeSpecification> m_proto;
    std::shared_ptr<AstArgumentList> m_arg_list;
    bool m_enable_constructor_call;

    /** Set while analyzing */
    std::shared_ptr<AstExpression> m_object_value;
    SymbolTypePtr_t m_instance_type;
    SymbolTypePtr_t m_prototype_type;
    std::shared_ptr<AstBlock> m_constructor_block; // create a block to store temporary vars
    std::shared_ptr<AstExpression> m_constructor_call;

    Pointer<AstNewExpression> CloneImpl() const
    {
        return Pointer<AstNewExpression>(new AstNewExpression(
            CloneAstNode(m_proto),
            CloneAstNode(m_arg_list),
            m_enable_constructor_call,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

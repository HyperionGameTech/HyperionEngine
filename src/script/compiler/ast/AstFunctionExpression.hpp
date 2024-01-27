#ifndef AST_FUNCTION_EXPRESSION_HPP
#define AST_FUNCTION_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <core/lib/String.hpp>

#include <memory>
#include <vector>

namespace hyperion::compiler {

class AstTypeExpression;

class AstFunctionExpression : public AstExpression
{
public:
    AstFunctionExpression(
        const Array<RC<AstParameter>> &parameters,
        const RC<AstPrototypeSpecification> &return_type_specification,
        const RC<AstBlock> &block,
        const SourceLocation &location
    );

    virtual ~AstFunctionExpression() override = default;

    bool IsConstructorDefinition() const
        { return m_is_constructor_definition; }

    void SetIsConstructorDefinition(bool is_constructor_definition)
        { m_is_constructor_definition = is_constructor_definition; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
    
    const SymbolTypePtr_t &GetReturnType() const
        { return m_return_type; }

    void SetReturnType(const SymbolTypePtr_t &return_type)
        { m_return_type = return_type; }

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstFunctionExpression>());
        
        for (auto &param : m_parameters) {
            hc.Add(param ? param->GetHashCode() : HashCode());
        }

        hc.Add(m_return_type_specification ? m_return_type_specification->GetHashCode() : HashCode());

        hc.Add(m_block ? m_block->GetHashCode() : HashCode());

        return hc;
    }

protected:
    Array<RC<AstParameter>>         m_parameters;
    RC<AstPrototypeSpecification>   m_return_type_specification;
    RC<AstBlock>                    m_block;

    Bool                            m_is_closure;

    RC<AstParameter>                m_closure_self_param;
    RC<AstPrototypeSpecification>   m_function_type_expr;
    RC<AstTypeExpression>           m_closure_type_expr;
    RC<AstBlock>                    m_block_with_parameters;

    Bool                            m_is_constructor_definition;

    SymbolTypePtr_t                 m_symbol_type;
    SymbolTypePtr_t                 m_return_type;

    Int                             m_closure_object_location;

    Int                             m_static_id;

    std::unique_ptr<Buildable> BuildFunctionBody(AstVisitor *visitor, Module *mod);
    RC<AstFunctionExpression> CloneImpl() const
    {
        return RC<AstFunctionExpression>(new AstFunctionExpression(
            CloneAllAstNodes(m_parameters),
            CloneAstNode(m_return_type_specification),
            CloneAstNode(m_block),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif

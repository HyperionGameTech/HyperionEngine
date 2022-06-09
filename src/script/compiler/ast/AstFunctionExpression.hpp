#ifndef AST_FUNCTION_EXPRESSION_HPP
#define AST_FUNCTION_EXPRESSION_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstParameter.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstTypeSpecification.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstObject.hpp>

#include <memory>
#include <vector>

namespace hyperion {
namespace compiler {

class AstFunctionExpression : public AstExpression {
public:
    AstFunctionExpression(
        const std::vector<std::shared_ptr<AstParameter>> &parameters,
        const std::shared_ptr<AstTypeSpecification> &type_specification,
        const std::shared_ptr<AstBlock> &block,
        bool is_async,
        bool is_pure,
        bool is_generator,
        const SourceLocation &location
    );
    virtual ~AstFunctionExpression() = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetSymbolType() const override;
    
    inline const SymbolTypePtr_t &GetReturnType() const { return m_return_type; }
    inline void SetReturnType(const SymbolTypePtr_t &return_type) { m_return_type = return_type; }

protected:
    std::vector<std::shared_ptr<AstParameter>> m_parameters;
    std::shared_ptr<AstTypeSpecification> m_type_specification;
    std::shared_ptr<AstBlock> m_block;
    bool m_is_async;
    bool m_is_pure;
    bool m_is_generator;
    bool m_is_closure;

    std::shared_ptr<AstExpression> m_closure_object;
    std::shared_ptr<AstParameter> m_closure_self_param;

    std::shared_ptr<AstFunctionExpression> m_generator_closure;
    bool m_is_generator_closure;

    SymbolTypePtr_t m_symbol_type;
    SymbolTypePtr_t m_return_type;
    SymbolTypePtr_t m_closure_type;

    int m_closure_object_location;

    int m_static_id;

    std::unique_ptr<Buildable> BuildFunctionBody(AstVisitor *visitor, Module *mod);

    inline void SetIsGeneratorClosure(bool is_generator_closure)
        { m_is_generator_closure = is_generator_closure; }

    inline Pointer<AstFunctionExpression> CloneImpl() const
    {
        return Pointer<AstFunctionExpression>(new AstFunctionExpression(
            CloneAllAstNodes(m_parameters),
            CloneAstNode(m_type_specification),
            CloneAstNode(m_block),
            m_is_async,
            m_is_pure,
            m_is_generator,
            m_location
        ));
    }
};

} // namespace compiler
} // namespace hyperion

#endif
